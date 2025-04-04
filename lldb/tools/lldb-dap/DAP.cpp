//===-- DAP.cpp -------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DAP.h"
#include "JSONUtils.h"
#include "LLDBUtils.h"
#include "OutputRedirector.h"
#include "lldb/API/SBBreakpoint.h"
#include "lldb/API/SBCommandInterpreter.h"
#include "lldb/API/SBCommandReturnObject.h"
#include "lldb/API/SBLanguageRuntime.h"
#include "lldb/API/SBListener.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBStream.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Utility/Status.h"
#include "lldb/lldb-defines.h"
#include "lldb/lldb-enumerations.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace lldb_dap;

namespace {
#ifdef _WIN32
const char DEV_NULL[] = "nul";
#else
const char DEV_NULL[] = "/dev/null";
#endif
} // namespace

namespace lldb_dap {

DAP::DAP(llvm::StringRef path, std::ofstream *log, ReplMode repl_mode,
         StreamDescriptor input, StreamDescriptor output)
    : debug_adaptor_path(path), log(log), input(std::move(input)),
      output(std::move(output)), broadcaster("lldb-dap"),
      exception_breakpoints(), focus_tid(LLDB_INVALID_THREAD_ID),
      stop_at_entry(false), is_attach(false),
      enable_auto_variable_summaries(false),
      enable_synthetic_child_debugging(false),
      display_extended_backtrace(false),
      restarting_process_id(LLDB_INVALID_PROCESS_ID),
      configuration_done_sent(false), waiting_for_run_in_terminal(false),
      progress_event_reporter(
          [&](const ProgressEvent &event) { SendJSON(event.ToJSON()); }),
      reverse_request_seq(0), repl_mode(repl_mode) {}

DAP::~DAP() = default;

/// Return string with first character capitalized.
static std::string capitalize(llvm::StringRef str) {
  if (str.empty())
    return "";
  return ((llvm::Twine)llvm::toUpper(str[0]) + str.drop_front()).str();
}

void DAP::PopulateExceptionBreakpoints() {
  llvm::call_once(init_exception_breakpoints_flag, [this]() {
    exception_breakpoints = std::vector<ExceptionBreakpoint>{};

    if (lldb::SBDebugger::SupportsLanguage(lldb::eLanguageTypeC_plus_plus)) {
      exception_breakpoints->emplace_back(*this, "cpp_catch", "C++ Catch",
                                          lldb::eLanguageTypeC_plus_plus);
      exception_breakpoints->emplace_back(*this, "cpp_throw", "C++ Throw",
                                          lldb::eLanguageTypeC_plus_plus);
    }
    if (lldb::SBDebugger::SupportsLanguage(lldb::eLanguageTypeObjC)) {
      exception_breakpoints->emplace_back(
          *this, "objc_catch", "Objective-C Catch", lldb::eLanguageTypeObjC);
      exception_breakpoints->emplace_back(
          *this, "objc_throw", "Objective-C Throw", lldb::eLanguageTypeObjC);
    }
    if (lldb::SBDebugger::SupportsLanguage(lldb::eLanguageTypeSwift)) {
      exception_breakpoints->emplace_back(*this, "swift_catch", "Swift Catch",
                                          lldb::eLanguageTypeSwift);
      exception_breakpoints->emplace_back(*this, "swift_throw", "Swift Throw",
                                          lldb::eLanguageTypeSwift);
    }
    // Besides handling the hardcoded list of languages from above, we try to
    // find any other languages that support exception breakpoints using the
    // SB API.
    for (int raw_lang = lldb::eLanguageTypeUnknown;
         raw_lang < lldb::eNumLanguageTypes; ++raw_lang) {
      lldb::LanguageType lang = static_cast<lldb::LanguageType>(raw_lang);

      // We first discard any languages already handled above.
      if (lldb::SBLanguageRuntime::LanguageIsCFamily(lang) ||
          lang == lldb::eLanguageTypeSwift)
        continue;

      if (!lldb::SBDebugger::SupportsLanguage(lang))
        continue;

      const char *name = lldb::SBLanguageRuntime::GetNameForLanguageType(lang);
      if (!name)
        continue;
      std::string raw_lang_name = name;
      std::string capitalized_lang_name = capitalize(name);

      if (lldb::SBLanguageRuntime::SupportsExceptionBreakpointsOnThrow(lang)) {
        const char *raw_throw_keyword =
            lldb::SBLanguageRuntime::GetThrowKeywordForLanguage(lang);
        std::string throw_keyword =
            raw_throw_keyword ? raw_throw_keyword : "throw";

        exception_breakpoints->emplace_back(
            *this, raw_lang_name + "_" + throw_keyword,
            capitalized_lang_name + " " + capitalize(throw_keyword), lang);
      }

      if (lldb::SBLanguageRuntime::SupportsExceptionBreakpointsOnCatch(lang)) {
        const char *raw_catch_keyword =
            lldb::SBLanguageRuntime::GetCatchKeywordForLanguage(lang);
        std::string catch_keyword =
            raw_catch_keyword ? raw_catch_keyword : "catch";

        exception_breakpoints->emplace_back(
            *this, raw_lang_name + "_" + catch_keyword,
            capitalized_lang_name + " " + capitalize(catch_keyword), lang);
      }
    }
    assert(!exception_breakpoints->empty() && "should not be empty");
  });
}

ExceptionBreakpoint *DAP::GetExceptionBreakpoint(const std::string &filter) {
  // PopulateExceptionBreakpoints() is called after g_dap.debugger is created
  // in a request-initialize.
  //
  // But this GetExceptionBreakpoint() method may be called before attaching, in
  // which case, we may not have populated the filter yet.
  //
  // We also cannot call PopulateExceptionBreakpoints() in DAP::DAP() because
  // we need SBDebugger::Initialize() to have been called before this.
  //
  // So just calling PopulateExceptionBreakoints(),which does lazy-populating
  // seems easiest. Two other options include:
  //  + call g_dap.PopulateExceptionBreakpoints() in lldb-dap.cpp::main()
  //    right after the call to SBDebugger::Initialize()
  //  + Just call PopulateExceptionBreakpoints() to get a fresh list  everytime
  //    we query (a bit overkill since it's not likely to change?)
  PopulateExceptionBreakpoints();

  for (auto &bp : *exception_breakpoints) {
    if (bp.filter == filter)
      return &bp;
  }
  return nullptr;
}

ExceptionBreakpoint *DAP::GetExceptionBreakpoint(const lldb::break_id_t bp_id) {
  // See comment in the other GetExceptionBreakpoint().
  PopulateExceptionBreakpoints();

  for (auto &bp : *exception_breakpoints) {
    if (bp.bp.GetID() == bp_id)
      return &bp;
  }
  return nullptr;
}

llvm::Error DAP::ConfigureIO(std::FILE *overrideOut, std::FILE *overrideErr) {
  in = lldb::SBFile(std::fopen(DEV_NULL, "r"), /*transfer_ownership=*/true);

  if (auto Error = out.RedirectTo([this](llvm::StringRef output) {
        SendOutput(OutputType::Stdout, output);
      }))
    return Error;

  if (overrideOut) {
    auto fd = out.GetWriteFileDescriptor();
    if (auto Error = fd.takeError())
      return Error;

    if (dup2(*fd, fileno(overrideOut)) == -1)
      return llvm::errorCodeToError(llvm::errnoAsErrorCode());
  }

  if (auto Error = err.RedirectTo([this](llvm::StringRef output) {
        SendOutput(OutputType::Stderr, output);
      }))
    return Error;

  if (overrideErr) {
    auto fd = err.GetWriteFileDescriptor();
    if (auto Error = fd.takeError())
      return Error;

    if (dup2(*fd, fileno(overrideErr)) == -1)
      return llvm::errorCodeToError(llvm::errnoAsErrorCode());
  }

  return llvm::Error::success();
}

void DAP::StopIO() {
  out.Stop();
  err.Stop();
}

// Send the JSON in "json_str" to the "out" stream. Correctly send the
// "Content-Length:" field followed by the length, followed by the raw
// JSON bytes.
void DAP::SendJSON(const std::string &json_str) {
  output.write_full("Content-Length: ");
  output.write_full(llvm::utostr(json_str.size()));
  output.write_full("\r\n\r\n");
  output.write_full(json_str);
}

// Serialize the JSON value into a string and send the JSON packet to
// the "out" stream.
void DAP::SendJSON(const llvm::json::Value &json) {
  std::string json_str;
  llvm::raw_string_ostream strm(json_str);
  strm << json;
  static std::mutex mutex;
  std::lock_guard<std::mutex> locker(mutex);
  SendJSON(json_str);

  if (log) {
    auto now = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch());
    *log << llvm::formatv("{0:f9} <-- ", now.count()).str() << std::endl
         << "Content-Length: " << json_str.size() << "\r\n\r\n"
         << llvm::formatv("{0:2}", json).str() << std::endl;
  }
}

// Read a JSON packet from the "in" stream.
std::string DAP::ReadJSON() {
  std::string length_str;
  std::string json_str;
  int length;

  if (!input.read_expected(log, "Content-Length: "))
    return json_str;

  if (!input.read_line(log, length_str))
    return json_str;

  if (!llvm::to_integer(length_str, length))
    return json_str;

  if (!input.read_expected(log, "\r\n"))
    return json_str;

  if (!input.read_full(log, length, json_str))
    return json_str;

  if (log) {
    auto now = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch());
    *log << llvm::formatv("{0:f9} --> ", now.count()).str() << std::endl
         << "Content-Length: " << length << "\r\n\r\n";
  }
  return json_str;
}

// "OutputEvent": {
//   "allOf": [ { "$ref": "#/definitions/Event" }, {
//     "type": "object",
//     "description": "Event message for 'output' event type. The event
//                     indicates that the target has produced some output.",
//     "properties": {
//       "event": {
//         "type": "string",
//         "enum": [ "output" ]
//       },
//       "body": {
//         "type": "object",
//         "properties": {
//           "category": {
//             "type": "string",
//             "description": "The output category. If not specified,
//                             'console' is assumed.",
//             "_enum": [ "console", "stdout", "stderr", "telemetry" ]
//           },
//           "output": {
//             "type": "string",
//             "description": "The output to report."
//           },
//           "variablesReference": {
//             "type": "number",
//             "description": "If an attribute 'variablesReference' exists
//                             and its value is > 0, the output contains
//                             objects which can be retrieved by passing
//                             variablesReference to the VariablesRequest."
//           },
//           "source": {
//             "$ref": "#/definitions/Source",
//             "description": "An optional source location where the output
//                             was produced."
//           },
//           "line": {
//             "type": "integer",
//             "description": "An optional source location line where the
//                             output was produced."
//           },
//           "column": {
//             "type": "integer",
//             "description": "An optional source location column where the
//                             output was produced."
//           },
//           "data": {
//             "type":["array","boolean","integer","null","number","object",
//                     "string"],
//             "description": "Optional data to report. For the 'telemetry'
//                             category the data will be sent to telemetry, for
//                             the other categories the data is shown in JSON
//                             format."
//           }
//         },
//         "required": ["output"]
//       }
//     },
//     "required": [ "event", "body" ]
//   }]
// }
void DAP::SendOutput(OutputType o, const llvm::StringRef output) {
  if (output.empty())
    return;

  const char *category = nullptr;
  switch (o) {
  case OutputType::Console:
    category = "console";
    break;
  case OutputType::Stdout:
    category = "stdout";
    break;
  case OutputType::Stderr:
    category = "stderr";
    break;
  case OutputType::Telemetry:
    category = "telemetry";
    break;
  }

  // Send each line of output as an individual event, including the newline if
  // present.
  ::size_t idx = 0;
  do {
    ::size_t end = output.find('\n', idx);
    if (end == llvm::StringRef::npos)
      end = output.size() - 1;
    llvm::json::Object event(CreateEventObject("output"));
    llvm::json::Object body;
    body.try_emplace("category", category);
    EmplaceSafeString(body, "output", output.slice(idx, end + 1).str());
    event.try_emplace("body", std::move(body));
    SendJSON(llvm::json::Value(std::move(event)));
    idx = end + 1;
  } while (idx < output.size());
}

// interface ProgressStartEvent extends Event {
//   event: 'progressStart';
//
//   body: {
//     /**
//      * An ID that must be used in subsequent 'progressUpdate' and
//      'progressEnd'
//      * events to make them refer to the same progress reporting.
//      * IDs must be unique within a debug session.
//      */
//     progressId: string;
//
//     /**
//      * Mandatory (short) title of the progress reporting. Shown in the UI to
//      * describe the long running operation.
//      */
//     title: string;
//
//     /**
//      * The request ID that this progress report is related to. If specified a
//      * debug adapter is expected to emit
//      * progress events for the long running request until the request has
//      been
//      * either completed or cancelled.
//      * If the request ID is omitted, the progress report is assumed to be
//      * related to some general activity of the debug adapter.
//      */
//     requestId?: number;
//
//     /**
//      * If true, the request that reports progress may be canceled with a
//      * 'cancel' request.
//      * So this property basically controls whether the client should use UX
//      that
//      * supports cancellation.
//      * Clients that don't support cancellation are allowed to ignore the
//      * setting.
//      */
//     cancellable?: boolean;
//
//     /**
//      * Optional, more detailed progress message.
//      */
//     message?: string;
//
//     /**
//      * Optional progress percentage to display (value range: 0 to 100). If
//      * omitted no percentage will be shown.
//      */
//     percentage?: number;
//   };
// }
//
// interface ProgressUpdateEvent extends Event {
//   event: 'progressUpdate';
//
//   body: {
//     /**
//      * The ID that was introduced in the initial 'progressStart' event.
//      */
//     progressId: string;
//
//     /**
//      * Optional, more detailed progress message. If omitted, the previous
//      * message (if any) is used.
//      */
//     message?: string;
//
//     /**
//      * Optional progress percentage to display (value range: 0 to 100). If
//      * omitted no percentage will be shown.
//      */
//     percentage?: number;
//   };
// }
//
// interface ProgressEndEvent extends Event {
//   event: 'progressEnd';
//
//   body: {
//     /**
//      * The ID that was introduced in the initial 'ProgressStartEvent'.
//      */
//     progressId: string;
//
//     /**
//      * Optional, more detailed progress message. If omitted, the previous
//      * message (if any) is used.
//      */
//     message?: string;
//   };
// }

void DAP::SendProgressEvent(uint64_t progress_id, const char *message,
                            uint64_t completed, uint64_t total) {
  progress_event_reporter.Push(progress_id, message, completed, total);
}

void __attribute__((format(printf, 3, 4)))
DAP::SendFormattedOutput(OutputType o, const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  int actual_length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  SendOutput(
      o, llvm::StringRef(buffer, std::min<int>(actual_length, sizeof(buffer))));
}

ExceptionBreakpoint *DAP::GetExceptionBPFromStopReason(lldb::SBThread &thread) {
  const auto num = thread.GetStopReasonDataCount();
  // Check to see if have hit an exception breakpoint and change the
  // reason to "exception", but only do so if all breakpoints that were
  // hit are exception breakpoints.
  ExceptionBreakpoint *exc_bp = nullptr;
  for (size_t i = 0; i < num; i += 2) {
    // thread.GetStopReasonDataAtIndex(i) will return the bp ID and
    // thread.GetStopReasonDataAtIndex(i+1) will return the location
    // within that breakpoint. We only care about the bp ID so we can
    // see if this is an exception breakpoint that is getting hit.
    lldb::break_id_t bp_id = thread.GetStopReasonDataAtIndex(i);
    exc_bp = GetExceptionBreakpoint(bp_id);
    // If any breakpoint is not an exception breakpoint, then stop and
    // report this as a normal breakpoint
    if (exc_bp == nullptr)
      return nullptr;
  }
  return exc_bp;
}

lldb::SBThread DAP::GetLLDBThread(const llvm::json::Object &arguments) {
  auto tid = GetSigned(arguments, "threadId", LLDB_INVALID_THREAD_ID);
  return target.GetProcess().GetThreadByID(tid);
}

lldb::SBFrame DAP::GetLLDBFrame(const llvm::json::Object &arguments) {
  const uint64_t frame_id = GetUnsigned(arguments, "frameId", UINT64_MAX);
  lldb::SBProcess process = target.GetProcess();
  // Upper 32 bits is the thread index ID
  lldb::SBThread thread =
      process.GetThreadByIndexID(GetLLDBThreadIndexID(frame_id));
  // Lower 32 bits is the frame index
  return thread.GetFrameAtIndex(GetLLDBFrameID(frame_id));
}

llvm::json::Value DAP::CreateTopLevelScopes() {
  llvm::json::Array scopes;
  scopes.emplace_back(
      CreateScope("Locals", VARREF_LOCALS, variables.locals.GetSize(), false));
  scopes.emplace_back(CreateScope("Globals", VARREF_GLOBALS,
                                  variables.globals.GetSize(), false));
  scopes.emplace_back(CreateScope("Registers", VARREF_REGS,
                                  variables.registers.GetSize(), false));
  return llvm::json::Value(std::move(scopes));
}

ReplMode DAP::DetectReplMode(lldb::SBFrame frame, std::string &expression,
                             bool partial_expression) {
  // Check for the escape hatch prefix.
  if (!expression.empty() &&
      llvm::StringRef(expression).starts_with(command_escape_prefix)) {
    expression = expression.substr(command_escape_prefix.size());
    return ReplMode::Command;
  }

  switch (repl_mode) {
  case ReplMode::Variable:
    return ReplMode::Variable;
  case ReplMode::Command:
    return ReplMode::Command;
  case ReplMode::Auto:
    // To determine if the expression is a command or not, check if the first
    // term is a variable or command. If it's a variable in scope we will prefer
    // that behavior and give a warning to the user if they meant to invoke the
    // operation as a command.
    //
    // Example use case:
    //   int p and expression "p + 1" > variable
    //   int i and expression "i" > variable
    //   int var and expression "va" > command
    std::pair<llvm::StringRef, llvm::StringRef> token =
        llvm::getToken(expression);

    // If the first token is not fully finished yet, we can't
    // determine whether this will be a variable or a lldb command.
    if (partial_expression && token.second.empty())
      return ReplMode::Auto;

    std::string term = token.first.str();
    lldb::SBCommandInterpreter interpreter = debugger.GetCommandInterpreter();
    bool term_is_command = interpreter.CommandExists(term.c_str()) ||
                           interpreter.UserCommandExists(term.c_str()) ||
                           interpreter.AliasExists(term.c_str());
    bool term_is_variable = frame.FindVariable(term.c_str()).IsValid();

    // If we have both a variable and command, warn the user about the conflict.
    if (term_is_command && term_is_variable) {
      llvm::errs()
          << "Warning: Expression '" << term
          << "' is both an LLDB command and variable. It will be evaluated as "
             "a variable. To evaluate the expression as an LLDB command, use '"
          << command_escape_prefix << "' as a prefix.\n";
    }

    // Variables take preference to commands in auto, since commands can always
    // be called using the command_escape_prefix
    return term_is_variable  ? ReplMode::Variable
           : term_is_command ? ReplMode::Command
                             : ReplMode::Variable;
  }

  llvm_unreachable("enum cases exhausted.");
}

bool DAP::RunLLDBCommands(llvm::StringRef prefix,
                          llvm::ArrayRef<std::string> commands) {
  bool required_command_failed = false;
  std::string output =
      ::RunLLDBCommands(debugger, prefix, commands, required_command_failed);
  SendOutput(OutputType::Console, output);
  return !required_command_failed;
}

static llvm::Error createRunLLDBCommandsErrorMessage(llvm::StringRef category) {
  return llvm::createStringError(
      llvm::inconvertibleErrorCode(),
      llvm::formatv(
          "Failed to run {0} commands. See the Debug Console for more details.",
          category)
          .str()
          .c_str());
}

llvm::Error
DAP::RunAttachCommands(llvm::ArrayRef<std::string> attach_commands) {
  if (!RunLLDBCommands("Running attachCommands:", attach_commands))
    return createRunLLDBCommandsErrorMessage("attach");
  return llvm::Error::success();
}

llvm::Error
DAP::RunLaunchCommands(llvm::ArrayRef<std::string> launch_commands) {
  if (!RunLLDBCommands("Running launchCommands:", launch_commands))
    return createRunLLDBCommandsErrorMessage("launch");
  return llvm::Error::success();
}

llvm::Error DAP::RunInitCommands() {
  if (!RunLLDBCommands("Running initCommands:", init_commands))
    return createRunLLDBCommandsErrorMessage("initCommands");
  return llvm::Error::success();
}

llvm::Error DAP::RunPreInitCommands() {
  if (!RunLLDBCommands("Running preInitCommands:", pre_init_commands))
    return createRunLLDBCommandsErrorMessage("preInitCommands");
  return llvm::Error::success();
}

llvm::Error DAP::RunPreRunCommands() {
  if (!RunLLDBCommands("Running preRunCommands:", pre_run_commands))
    return createRunLLDBCommandsErrorMessage("preRunCommands");
  return llvm::Error::success();
}

void DAP::RunPostRunCommands() {
  RunLLDBCommands("Running postRunCommands:", post_run_commands);
}
void DAP::RunStopCommands() {
  RunLLDBCommands("Running stopCommands:", stop_commands);
}

void DAP::RunExitCommands() {
  RunLLDBCommands("Running exitCommands:", exit_commands);
}

void DAP::RunTerminateCommands() {
  RunLLDBCommands("Running terminateCommands:", terminate_commands);
}

lldb::SBTarget
DAP::CreateTargetFromArguments(const llvm::json::Object &arguments,
                               lldb::SBError &error) {
  // Grab the name of the program we need to debug and create a target using
  // the given program as an argument. Executable file can be a source of target
  // architecture and platform, if they differ from the host. Setting exe path
  // in launch info is useless because Target.Launch() will not change
  // architecture and platform, therefore they should be known at the target
  // creation. We also use target triple and platform from the launch
  // configuration, if given, since in some cases ELF file doesn't contain
  // enough information to determine correct arch and platform (or ELF can be
  // omitted at all), so it is good to leave the user an apportunity to specify
  // those. Any of those three can be left empty.
  llvm::StringRef target_triple = GetString(arguments, "targetTriple");
  llvm::StringRef platform_name = GetString(arguments, "platformName");
  llvm::StringRef program = GetString(arguments, "program");
  auto target = this->debugger.CreateTarget(
      program.data(), target_triple.data(), platform_name.data(),
      true, // Add dependent modules.
      error);

  if (error.Fail()) {
    // Update message if there was an error.
    error.SetErrorStringWithFormat(
        "Could not create a target for a program '%s': %s.", program.data(),
        error.GetCString());
  }

  return target;
}

void DAP::SetTarget(const lldb::SBTarget target) {
  this->target = target;

  if (target.IsValid()) {
    // Configure breakpoint event listeners for the target.
    lldb::SBListener listener = this->debugger.GetListener();
    listener.StartListeningForEvents(
        this->target.GetBroadcaster(),
        lldb::SBTarget::eBroadcastBitBreakpointChanged);
    listener.StartListeningForEvents(this->broadcaster,
                                     eBroadcastBitStopEventThread);
  }
}

PacketStatus DAP::GetNextObject(llvm::json::Object &object) {
  std::string json = ReadJSON();
  if (json.empty())
    return PacketStatus::EndOfFile;

  llvm::StringRef json_sref(json);
  llvm::Expected<llvm::json::Value> json_value = llvm::json::parse(json_sref);
  if (!json_value) {
    auto error = json_value.takeError();
    if (log) {
      std::string error_str;
      llvm::raw_string_ostream strm(error_str);
      strm << error;
      *log << "error: failed to parse JSON: " << error_str << std::endl
           << json << std::endl;
    }
    return PacketStatus::JSONMalformed;
  }

  if (log) {
    *log << llvm::formatv("{0:2}", *json_value).str() << std::endl;
  }

  llvm::json::Object *object_ptr = json_value->getAsObject();
  if (!object_ptr) {
    if (log)
      *log << "error: json packet isn't a object" << std::endl;
    return PacketStatus::JSONNotObject;
  }
  object = *object_ptr;
  return PacketStatus::Success;
}

bool DAP::HandleObject(const llvm::json::Object &object) {
  const auto packet_type = GetString(object, "type");
  if (packet_type == "request") {
    const auto command = GetString(object, "command");
    auto handler_pos = request_handlers.find(command);
    if (handler_pos == request_handlers.end()) {
      if (log)
        *log << "error: unhandled command \"" << command.data() << "\""
             << std::endl;
      return false; // Fail
    }

    handler_pos->second(*this, object);
    return true; // Success
  }

  if (packet_type == "response") {
    auto id = GetSigned(object, "request_seq", 0);
    ResponseCallback response_handler = [](llvm::Expected<llvm::json::Value>) {
      llvm::errs() << "Unhandled response\n";
    };

    {
      std::lock_guard<std::mutex> locker(call_mutex);
      auto inflight = inflight_reverse_requests.find(id);
      if (inflight != inflight_reverse_requests.end()) {
        response_handler = std::move(inflight->second);
        inflight_reverse_requests.erase(inflight);
      }
    }

    // Result should be given, use null if not.
    if (GetBoolean(object, "success", false)) {
      llvm::json::Value Result = nullptr;
      if (auto *B = object.get("body")) {
        Result = std::move(*B);
      }
      response_handler(Result);
    } else {
      llvm::StringRef message = GetString(object, "message");
      if (message.empty()) {
        message = "Unknown error, response failed";
      }
      response_handler(llvm::createStringError(
          std::error_code(-1, std::generic_category()), message));
    }

    return true;
  }

  return false;
}

llvm::Error DAP::Loop() {
  while (!disconnecting) {
    llvm::json::Object object;
    lldb_dap::PacketStatus status = GetNextObject(object);

    if (status == lldb_dap::PacketStatus::EndOfFile) {
      break;
    }

    if (status != lldb_dap::PacketStatus::Success) {
      return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                     "failed to send packet");
    }

    if (!HandleObject(object)) {
      return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                     "unhandled packet");
    }
  }

  return llvm::Error::success();
}

void DAP::SendReverseRequest(llvm::StringRef command,
                             llvm::json::Value arguments,
                             ResponseCallback callback) {
  int64_t id;
  {
    std::lock_guard<std::mutex> locker(call_mutex);
    id = ++reverse_request_seq;
    inflight_reverse_requests.emplace(id, std::move(callback));
  }

  SendJSON(llvm::json::Object{
      {"type", "request"},
      {"seq", id},
      {"command", command},
      {"arguments", std::move(arguments)},
  });
}

void DAP::RegisterRequestCallback(std::string request,
                                  RequestCallback callback) {
  request_handlers[request] = callback;
}

lldb::SBError DAP::WaitForProcessToStop(uint32_t seconds) {
  lldb::SBError error;
  lldb::SBProcess process = target.GetProcess();
  if (!process.IsValid()) {
    error.SetErrorString("invalid process");
    return error;
  }
  auto timeout_time =
      std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (std::chrono::steady_clock::now() < timeout_time) {
    const auto state = process.GetState();
    switch (state) {
    case lldb::eStateAttaching:
    case lldb::eStateConnected:
    case lldb::eStateInvalid:
    case lldb::eStateLaunching:
    case lldb::eStateRunning:
    case lldb::eStateStepping:
    case lldb::eStateSuspended:
      break;
    case lldb::eStateDetached:
      error.SetErrorString("process detached during launch or attach");
      return error;
    case lldb::eStateExited:
      error.SetErrorString("process exited during launch or attach");
      return error;
    case lldb::eStateUnloaded:
      error.SetErrorString("process unloaded during launch or attach");
      return error;
    case lldb::eStateCrashed:
    case lldb::eStateStopped:
      return lldb::SBError(); // Success!
    }
    std::this_thread::sleep_for(std::chrono::microseconds(250));
  }
  error.SetErrorStringWithFormat("process failed to stop within %u seconds",
                                 seconds);
  return error;
}

void Variables::Clear() {
  locals.Clear();
  globals.Clear();
  registers.Clear();
  referenced_variables.clear();
}

int64_t Variables::GetNewVariableReference(bool is_permanent) {
  if (is_permanent)
    return next_permanent_var_ref++;
  return next_temporary_var_ref++;
}

bool Variables::IsPermanentVariableReference(int64_t var_ref) {
  return var_ref >= PermanentVariableStartIndex;
}

lldb::SBValue Variables::GetVariable(int64_t var_ref) const {
  if (IsPermanentVariableReference(var_ref)) {
    auto pos = referenced_permanent_variables.find(var_ref);
    if (pos != referenced_permanent_variables.end())
      return pos->second;
  } else {
    auto pos = referenced_variables.find(var_ref);
    if (pos != referenced_variables.end())
      return pos->second;
  }
  return lldb::SBValue();
}

int64_t Variables::InsertVariable(lldb::SBValue variable, bool is_permanent) {
  int64_t var_ref = GetNewVariableReference(is_permanent);
  if (is_permanent)
    referenced_permanent_variables.insert(std::make_pair(var_ref, variable));
  else
    referenced_variables.insert(std::make_pair(var_ref, variable));
  return var_ref;
}

bool StartDebuggingRequestHandler::DoExecute(
    lldb::SBDebugger debugger, char **command,
    lldb::SBCommandReturnObject &result) {
  // Command format like: `start-debugging <launch|attach> <configuration>`
  if (!command) {
    result.SetError("Invalid use of start-debugging, expected format "
                    "`start-debugging <launch|attach> <configuration>`.");
    return false;
  }

  if (!command[0] || llvm::StringRef(command[0]).empty()) {
    result.SetError("start-debugging request type missing.");
    return false;
  }

  if (!command[1] || llvm::StringRef(command[1]).empty()) {
    result.SetError("start-debugging debug configuration missing.");
    return false;
  }

  llvm::StringRef request{command[0]};
  std::string raw_configuration{command[1]};

  llvm::Expected<llvm::json::Value> configuration =
      llvm::json::parse(raw_configuration);

  if (!configuration) {
    llvm::Error err = configuration.takeError();
    std::string msg = "Failed to parse json configuration: " +
                      llvm::toString(std::move(err)) + "\n\n" +
                      raw_configuration;
    result.SetError(msg.c_str());
    return false;
  }

  dap.SendReverseRequest(
      "startDebugging",
      llvm::json::Object{{"request", request},
                         {"configuration", std::move(*configuration)}},
      [](llvm::Expected<llvm::json::Value> value) {
        if (!value) {
          llvm::Error err = value.takeError();
          llvm::errs() << "reverse start debugging request failed: "
                       << llvm::toString(std::move(err)) << "\n";
        }
      });

  result.SetStatus(lldb::eReturnStatusSuccessFinishNoResult);

  return true;
}

bool ReplModeRequestHandler::DoExecute(lldb::SBDebugger debugger,
                                       char **command,
                                       lldb::SBCommandReturnObject &result) {
  // Command format like: `repl-mode <variable|command|auto>?`
  // If a new mode is not specified report the current mode.
  if (!command || llvm::StringRef(command[0]).empty()) {
    std::string mode;
    switch (dap.repl_mode) {
    case ReplMode::Variable:
      mode = "variable";
      break;
    case ReplMode::Command:
      mode = "command";
      break;
    case ReplMode::Auto:
      mode = "auto";
      break;
    }

    result.Printf("lldb-dap repl-mode %s.\n", mode.c_str());
    result.SetStatus(lldb::eReturnStatusSuccessFinishResult);

    return true;
  }

  llvm::StringRef new_mode{command[0]};

  if (new_mode == "variable") {
    dap.repl_mode = ReplMode::Variable;
  } else if (new_mode == "command") {
    dap.repl_mode = ReplMode::Command;
  } else if (new_mode == "auto") {
    dap.repl_mode = ReplMode::Auto;
  } else {
    lldb::SBStream error_message;
    error_message.Printf("Invalid repl-mode '%s'. Expected one of 'variable', "
                         "'command' or 'auto'.\n",
                         new_mode.data());
    result.SetError(error_message.GetData());
    return false;
  }

  result.Printf("lldb-dap repl-mode %s set.\n", new_mode.data());
  result.SetStatus(lldb::eReturnStatusSuccessFinishNoResult);
  return true;
}

// Sends a DAP event with an optional body.
//
// See
// https://code.visualstudio.com/api/references/vscode-api#debug.onDidReceiveDebugSessionCustomEvent
bool SendEventRequestHandler::DoExecute(lldb::SBDebugger debugger,
                                        char **command,
                                        lldb::SBCommandReturnObject &result) {
  // Command format like: `send-event <name> <body>?`
  if (!command || !command[0] || llvm::StringRef(command[0]).empty()) {
    result.SetError("Not enough arguments found, expected format "
                    "`lldb-dap send-event <name> <body>?`.");
    return false;
  }

  llvm::StringRef name{command[0]};
  // Events that are stateful and should be handled by lldb-dap internally.
  const std::array internal_events{"breakpoint", "capabilities", "continued",
                                   "exited",     "initialize",   "loadedSource",
                                   "module",     "process",      "stopped",
                                   "terminated", "thread"};
  if (std::find(internal_events.begin(), internal_events.end(), name) !=
      std::end(internal_events)) {
    std::string msg =
        llvm::formatv("Invalid use of lldb-dap send-event, event \"{0}\" "
                      "should be handled by lldb-dap internally.",
                      name)
            .str();
    result.SetError(msg.c_str());
    return false;
  }

  llvm::json::Object event(CreateEventObject(name));

  if (command[1] && !llvm::StringRef(command[1]).empty()) {
    // See if we have unused arguments.
    if (command[2]) {
      result.SetError(
          "Additional arguments found, expected `lldb-dap send-event "
          "<name> <body>?`.");
      return false;
    }

    llvm::StringRef raw_body{command[1]};

    llvm::Expected<llvm::json::Value> body = llvm::json::parse(raw_body);

    if (!body) {
      llvm::Error err = body.takeError();
      std::string msg = "Failed to parse custom event body: " +
                        llvm::toString(std::move(err));
      result.SetError(msg.c_str());
      return false;
    }

    event.try_emplace("body", std::move(*body));
  }

  dap.SendJSON(llvm::json::Value(std::move(event)));
  result.SetStatus(lldb::eReturnStatusSuccessFinishNoResult);
  return true;
}

void DAP::SetFrameFormat(llvm::StringRef format) {
  if (format.empty())
    return;
  lldb::SBError error;
  frame_format = lldb::SBFormat(format.str().c_str(), error);
  if (error.Fail()) {
    SendOutput(OutputType::Console,
               llvm::formatv(
                   "The provided frame format '{0}' couldn't be parsed: {1}\n",
                   format, error.GetCString())
                   .str());
  }
}

void DAP::SetThreadFormat(llvm::StringRef format) {
  if (format.empty())
    return;
  lldb::SBError error;
  thread_format = lldb::SBFormat(format.str().c_str(), error);
  if (error.Fail()) {
    SendOutput(OutputType::Console,
               llvm::formatv(
                   "The provided thread format '{0}' couldn't be parsed: {1}\n",
                   format, error.GetCString())
                   .str());
  }
}

InstructionBreakpoint *
DAP::GetInstructionBreakpoint(const lldb::break_id_t bp_id) {
  for (auto &bp : instruction_breakpoints) {
    if (bp.second.bp.GetID() == bp_id)
      return &bp.second;
  }
  return nullptr;
}

InstructionBreakpoint *
DAP::GetInstructionBPFromStopReason(lldb::SBThread &thread) {
  const auto num = thread.GetStopReasonDataCount();
  InstructionBreakpoint *inst_bp = nullptr;
  for (size_t i = 0; i < num; i += 2) {
    // thread.GetStopReasonDataAtIndex(i) will return the bp ID and
    // thread.GetStopReasonDataAtIndex(i+1) will return the location
    // within that breakpoint. We only care about the bp ID so we can
    // see if this is an instruction breakpoint that is getting hit.
    lldb::break_id_t bp_id = thread.GetStopReasonDataAtIndex(i);
    inst_bp = GetInstructionBreakpoint(bp_id);
    // If any breakpoint is not an instruction breakpoint, then stop and
    // report this as a normal breakpoint
    if (inst_bp == nullptr)
      return nullptr;
  }
  return inst_bp;
}

} // namespace lldb_dap
