====================================================
Extra Clang Tools |release| |ReleaseNotesTitle|
====================================================

.. contents::
   :local:
   :depth: 3

Written by the `LLVM Team <https://llvm.org/>`_

.. only:: PreRelease

  .. warning::
     These are in-progress notes for the upcoming Extra Clang Tools |version| release.
     Release notes for previous releases can be found on
     `the Download Page <https://releases.llvm.org/download.html>`_.

Introduction
============

This document contains the release notes for the Extra Clang Tools, part of the
Clang release |release|. Here we describe the status of the Extra Clang Tools in
some detail, including major improvements from the previous release and new
feature work. All LLVM releases may be downloaded from the `LLVM releases web
site <https://llvm.org/releases/>`_.

For more information about Clang or LLVM, including information about
the latest release, please see the `Clang Web Site <https://clang.llvm.org>`_ or
the `LLVM Web Site <https://llvm.org>`_.

Note that if you are reading this file from a Git checkout or the
main Clang web page, this document applies to the *next* release, not
the current one. To see the release notes for a specific release, please
see the `releases page <https://llvm.org/releases/>`_.

What's New in Extra Clang Tools |release|?
==========================================

Some of the major new features and improvements to Extra Clang Tools are listed
here. Generic improvements to Extra Clang Tools as a whole or to its underlying
infrastructure are described first, followed by tool-specific sections.

Major New Features
------------------

- The ``clang-pseudo`` tool is incomplete and does not have active maintainers,
  so it has been removed. See
  `the RFC <https://discourse.llvm.org/t/removing-pseudo-parser/71131/>`_ for
  more details.

...

Improvements to clangd
----------------------

Inlay hints
^^^^^^^^^^^

- Added support for inlay hints for default arguments, enabled using the
  `DefaultArguments` config option (#GH95712)

Diagnostics
^^^^^^^^^^^

Semantic Highlighting
^^^^^^^^^^^^^^^^^^^^^

Compile flags
^^^^^^^^^^^^^

- Fixed a bug where clangd would unnecessarily reparse open files whose
  compile command did not change when receiving a new compile command
  via an LSP `workspace/configuration` request (#GH115438)

Hover
^^^^^

- Hovering over a function name now shows the function's documentation
  comment even if the comment is written above the function's out-of-line
  definition in a different source file (#GH67802)

Code completion
^^^^^^^^^^^^^^^

- Added an `ArgumentLists` config option under `Completion`. This is a more
  flexible version of the `--function-arg-placeholders` command line flag,
  allowing users more detailed control of what is inserted in argument list
  position when clangd completes the name of a function in a function call
  context. (#GH111322)
- Clangd now supports configuring which headers should be inserted using 
  `<>` vs. `""` syntax using the `QuotedHeaders` and `AngledHeaders` config
  options under `Style` (#GH67749)
- Added completion for C++20 keywords.
- Improved code completion behaviour in dependent/templated code
- Completion items now include documentation comments in more cases (#GH120099)

Code actions
^^^^^^^^^^^^

- Added `Swap operands` tweak for certain binary operators.
- Improved the extract-to-function code action to allow extracting statements
  with overloaded operators like ``<<`` of ``std::ostream``.
- `Define outline` now handles member functions of class templates, and
  member function templates.
- `Extract variable` can now operate on the top-level expression in an
  expression statement (#GH112525)

Signature help
^^^^^^^^^^^^^^

Cross-references
^^^^^^^^^^^^^^^^

- Clangd now supports the "outgoing calls" direction of call hierarchy
  (#GH77556)
- Call hierarchy can now be invoked on fields and namespace-scope
  variables (#GH113900)
- Improved heuristics for filtering out generated Protobuf symbol names
  during indexing (#GH110091)
- Compiler intrinsics defined in `*intrin.h` system headers are now
  indexed even if they have reserved names (#GH119735)
- Various improvements to go-to-definition in templated code

Objective-C
^^^^^^^^^^^

Clang-tidy integration
^^^^^^^^^^^^^^^^^^^^^^

- Improved robustness in handling clang-tidy check names (#GH109421)

C++20 Modules Support
^^^^^^^^^^^^^^^^^^^^^

- Support code completion for symbols defined in modules (#GH110083)
- Improve performance when opening files that import modules (#GH106683)
- Compile commands for modules now respect modifications specified in `.clangd`
  files (#GH122606)

Miscellaneous
^^^^^^^^^^^^^

- Fixed an OOM affecting some versions of libcxx headers compiled in C++20
  mode (#GH108866)
- Various other stability improvements, e.g. crash fixes

Improvements to clang-doc
-------------------------

Improvements to clang-query
---------------------------

- Added `set enable-profile true/false` command for basic matcher profiling.

Improvements to clang-tidy
--------------------------

- Improved :program:`clang-tidy-diff.py` script. Add the `-only-check-in-db`
  option to exclude files not present in the compilation database, avoiding
  false-negative results.

- Improved :program:`run-clang-tidy.py` script. Fixed minor shutdown noise
  happening on certain platforms when interrupting the script.

- Improved :program:`clang-tidy`:

  - add support for `--verify-config` flag to check the configuration options of
    the `Clang Static Analyzer Checks <https://clang.llvm.org/docs/analyzer/checkers.html>`_.
  - accept parameters file in command line.
  - fix incorrect configuration file path resolving when file paths contain ``..``.
  - remove global options for most of checks. All options are changed to local
    options except `IncludeStyle`, `StrictMode` and `IgnoreMacros`. Global scoped
    `StrictMode` and `IgnoreMacros` are deprecated and will be removed in further
    releases.

.. csv-table::
  :header: "Check", "Options removed from global option"

  :doc:`bugprone-reserved-identifier <clang-tidy/checks/bugprone/reserved-identifier>`, AggressiveDependentMemberLookup
  :doc:`bugprone-unchecked-optional-access <clang-tidy/checks/bugprone/unchecked-optional-access>`, IgnoreSmartPointerDereference
  :doc:`cppcoreguidelines-pro-type-member-init <clang-tidy/checks/cppcoreguidelines/pro-type-member-init>`, UseAssignment
  :doc:`cppcoreguidelines-rvalue-reference-param-not-moved <clang-tidy/checks/cppcoreguidelines/rvalue-reference-param-not-moved>`, AllowPartialMove; IgnoreUnnamedParams; IgnoreNonDeducedTemplateTypes
  :doc:`misc-include-cleaner <clang-tidy/checks/misc/include-cleaner>`, IgnoreHeaders; DeduplicateFindings
  :doc:`performance-inefficient-vector-operation <clang-tidy/checks/performance/inefficient-vector-operation>`, EnableProto
  :doc:`readability-identifier-naming <clang-tidy/checks/readability/identifier-naming>`, AggressiveDependentMemberLookup
  :doc:`readability-inconsistent-declaration-parameter-name <clang-tidy/checks/readability/inconsistent-declaration-parameter-name>`, Strict
  :doc:`readability-redundant-access-specifiers <clang-tidy/checks/readability/redundant-access-specifiers>`, CheckFirstDeclaration
  :doc:`readability-redundant-casting <clang-tidy/checks/readability/redundant-casting>`, IgnoreTypeAliases

- Fixed bug in :program:`clang-tidy` by which `HeaderFilterRegex` did not take
  effect when passed via the `.clang-tidy` file.

- Fixed bug in :program:`run_clang_tidy.py` where the program would not
  correctly display the checks enabled by the top-level `.clang-tidy` file.

New checks
^^^^^^^^^^

- New :doc:`bugprone-bitwise-pointer-cast
  <clang-tidy/checks/bugprone/bitwise-pointer-cast>` check.

  Warns about code that tries to cast between pointers by means of
  ``std::bit_cast`` or ``memcpy``.

- New :doc:`bugprone-incorrect-enable-shared-from-this
  <clang-tidy/checks/bugprone/incorrect-enable-shared-from-this>` check.

  Detect classes or structs that do not publicly inherit from 
  ``std::enable_shared_from_this``, because unintended behavior will 
  otherwise occur when calling ``shared_from_this``.
  
- New :doc:`bugprone-nondeterministic-pointer-iteration-order
  <clang-tidy/checks/bugprone/nondeterministic-pointer-iteration-order>`
  check.

  Finds nondeterministic usages of pointers in unordered containers.

- New :doc:`bugprone-tagged-union-member-count
  <clang-tidy/checks/bugprone/tagged-union-member-count>` check.

  Gives warnings for tagged unions, where the number of tags is
  different from the number of data members inside the union.

- New :doc:`modernize-use-integer-sign-comparison
  <clang-tidy/checks/modernize/use-integer-sign-comparison>` check.

  Replace comparisons between signed and unsigned integers with their safe
  C++20 ``std::cmp_*`` alternative, if available.

- New :doc:`portability-template-virtual-member-function
  <clang-tidy/checks/portability/template-virtual-member-function>` check.

  Finds cases when an uninstantiated virtual member function in a template class
  causes cross-compiler incompatibility.

New check aliases
^^^^^^^^^^^^^^^^^

- New alias :doc:`cert-arr39-c <clang-tidy/checks/cert/arr39-c>` to
  :doc:`bugprone-sizeof-expression
  <clang-tidy/checks/bugprone/sizeof-expression>` was added.

Changes in existing checks
^^^^^^^^^^^^^^^^^^^^^^^^^^

- Improved :doc:`altera-id-dependent-backward-branch
  <clang-tidy/checks/altera/id-dependent-backward-branch>` check by fixing
  crashes from invalid code.

- Improved :doc:`bugprone-branch-clone
  <clang-tidy/checks/bugprone/branch-clone>` check to improve detection of
  branch clones by now detecting duplicate inner and outer if statements.

- Improved :doc:`bugprone-casting-through-void
  <clang-tidy/checks/bugprone/casting-through-void>` check to suggest replacing
  the offending code with ``reinterpret_cast``, to more clearly express intent.

- Improved :doc:`bugprone-dangling-handle
  <clang-tidy/checks/bugprone/dangling-handle>` check to treat ``std::span`` as a
  handle class.

- Improved :doc:`bugprone-exception-escape
  <clang-tidy/checks/bugprone/exception-escape>` by fixing false positives
  when a consteval function with throw statements.

- Improved :doc:`bugprone-forwarding-reference-overload
  <clang-tidy/checks/bugprone/forwarding-reference-overload>` check by fixing
  a crash when determining if an ``enable_if[_t]`` was found.

- Improve :doc:`bugprone-narrowing-conversions
  <clang-tidy/checks/bugprone/narrowing-conversions>` to avoid incorrect check
  results when floating point type is not ``float``, ``double`` and
  ``long double``.

- Improved :doc:`bugprone-optional-value-conversion
  <clang-tidy/checks/bugprone/optional-value-conversion>` to support detecting
  conversion directly by ``std::make_unique`` and ``std::make_shared``.

- Improved :doc:`bugprone-posix-return
  <clang-tidy/checks/bugprone/posix-return>` check to support integer literals
  as LHS and posix call as RHS of comparison.

- Improved :doc:`bugprone-return-const-ref-from-parameter
  <clang-tidy/checks/bugprone/return-const-ref-from-parameter>` check to
  diagnose potential dangling references when returning a ``const &`` parameter
  by using the conditional operator ``cond ? var1 : var2`` and fixing false
  positives for functions which contain lambda and ignore parameters
  with ``[[clang::lifetimebound]]`` attribute.
  
- Improved :doc:`bugprone-sizeof-expression
  <clang-tidy/checks/bugprone/sizeof-expression>` check to find suspicious
  usages of ``sizeof()``, ``alignof()``, and ``offsetof()`` when adding or
  subtracting from a pointer directly or when used to scale a numeric value and
  fix false positive when sizeof expression with template types.

- Improved :doc:`bugprone-throw-keyword-missing
  <clang-tidy/checks/bugprone/throw-keyword-missing>` by fixing a false positive
  when using non-static member initializers and a constructor.

- Improved :doc:`bugprone-unchecked-optional-access
  <clang-tidy/checks/bugprone/unchecked-optional-access>` to support
  ``bsl::optional`` and ``bdlb::NullableValue`` from
  <https://github.com/bloomberg/bde>_.

- Improved :doc:`bugprone-unhandled-self-assignment
  <clang-tidy/checks/bugprone/unhandled-self-assignment>` check by fixing smart
  pointer check against std::unique_ptr type.

- Improved :doc:`bugprone-unsafe-functions
  <clang-tidy/checks/bugprone/unsafe-functions>` check to allow specifying
  additional functions to match.

- Improved :doc:`bugprone-unused-local-non-trivial-variable
  <clang-tidy/checks/bugprone/unused-local-non-trivial-variable>` check to avoid
  false positives when using name-independent variables after C++26.

- Improved :doc:`bugprone-use-after-move
  <clang-tidy/checks/bugprone/use-after-move>` to avoid triggering on
  ``reset()`` calls on moved-from ``std::optional`` and ``std::any`` objects,
  similarly to smart pointers.

- Improved :doc:`cert-flp30-c <clang-tidy/checks/cert/flp30-c>` check to
  fix false positive that floating point variable is only used in increment
  expression.

- Improved :doc:`cppcoreguidelines-avoid-const-or-ref-data-members
  <clang-tidy/checks/cppcoreguidelines/avoid-const-or-ref-data-members>` check to
  avoid false positives when detecting a templated class with inheritance.

- Improved :doc:`cppcoreguidelines-init-variables
  <clang-tidy/checks/cppcoreguidelines/init-variables>` check by fixing the
  insertion location for function pointers.

- Improved :doc:`cppcoreguidelines-prefer-member-initializer
  <clang-tidy/checks/cppcoreguidelines/prefer-member-initializer>` check to
  avoid false positive when member initialization depends on a structured
  binding variable.

- Fixed :doc:`cppcoreguidelines-pro-type-union-access
  <clang-tidy/checks/cppcoreguidelines/pro-type-union-access>` check to
  report a location even when the member location is not valid.

- Improved :doc:`misc-definitions-in-headers
  <clang-tidy/checks/misc/definitions-in-headers>` check by rewording the
  diagnostic note that suggests adding ``inline``.

- Improved :doc:`misc-redundant-expression
  <clang-tidy/checks/misc/redundant-expression>` check by extending the
  checker to detect floating point and integer literals in redundant
  expressions.

- Improved :doc:`misc-unconventional-assign-operator
  <clang-tidy/checks/misc/unconventional-assign-operator>` check to avoid
  false positive for C++23 deducing this.

- Improved :doc:`misc-use-internal-linkage
  <clang-tidy/checks/misc/use-internal-linkage>` check to insert ``static``
  keyword before type qualifiers such as ``const`` and ``volatile``. Also, fix
  false positives for function declaration without body, C++20 consteval
  functions, C++20 export declarations, and global scoped
  overloaded ``operator new`` and ``operator delete``.

- Improved :doc:`modernize-avoid-c-arrays
  <clang-tidy/checks/modernize/avoid-c-arrays>` check to suggest using
  ``std::span`` as a replacement for parameters of incomplete C array type in
  C++20 and ``std::array`` or ``std::vector`` before C++20.

- Improved :doc:`modernize-loop-convert
  <clang-tidy/checks/modernize/loop-convert>` check to fix false positive when
  using loop variable in initializer of lambda capture.

- Improved :doc:`modernize-min-max-use-initializer-list
  <clang-tidy/checks/modernize/min-max-use-initializer-list>` check by fixing
  a false positive when only an implicit conversion happened inside an
  initializer list.

- Improved :doc:`modernize-raw-string-literal
  <clang-tidy/checks/modernize/raw-string-literal>` check to fix incorrect
  fix-it when the string contains a user-defined suffix.

- Improved :doc:`modernize-use-designated-initializers
  <clang-tidy/checks/modernize/use-designated-initializers>` check to fix a
  crash when a class is declared but not defined.

- Improved :doc:`modernize-use-integer-sign-comparison
  <clang-tidy/checks/modernize/use-integer-sign-comparison>` check to
  add an option ``EnableQtSupport``, that makes C++17 ``q20::cmp_*`` alternative
  available for Qt-based applications.

- Improved :doc:`modernize-use-nullptr
  <clang-tidy/checks/modernize/use-nullptr>` check to also recognize
  ``NULL``/``__null`` (but not ``0``) when used with a templated type.

- Improved :doc:`modernize-use-starts-ends-with
  <clang-tidy/checks/modernize/use-starts-ends-with>` check to handle two new
  cases from ``rfind`` and ``compare`` to ``ends_with``, and one new case from
  ``substr`` to ``starts_with``, and a small adjustment to the  diagnostic message.

- Improved :doc:`modernize-use-std-format
  <clang-tidy/checks/modernize/use-std-format>` check to support replacing
  member function calls too and to only expand macros starting with ``PRI``
  and ``__PRI`` from ``<inttypes.h>`` in the format string.

- Improved :doc:`modernize-use-std-print
  <clang-tidy/checks/modernize/use-std-print>` check to support replacing
  member function calls too and to only expand macros starting with ``PRI``
  and ``__PRI`` from ``<inttypes.h>`` in the format string.

- Improved :doc:`modernize-use-using
  <clang-tidy/checks/modernize/use-using>` check by not expanding macros.

- Improved :doc:`performance-avoid-endl
  <clang-tidy/checks/performance/avoid-endl>` check to use ``std::endl`` as
  placeholder when lexer cannot get source text.

- Improved :doc:`performance-move-const-arg
  <clang-tidy/checks/performance/move-const-arg>` check to fix a crash when
  an argument type is declared but not defined.

- Improved :doc:`performance-unnecessary-copy-initialization
  <clang-tidy/checks/performance/unnecessary-copy-initialization>` check
  to consider static member functions the same way as free functions.

- Improved :doc:`readability-container-contains
  <clang-tidy/checks/readability/container-contains>` check to let it work on
  any class that has a ``contains`` method. Fix some false negatives in the
  ``find()`` case.

- Improved :doc:`readability-enum-initial-value
  <clang-tidy/checks/readability/enum-initial-value>` check by only issuing
  diagnostics for the definition of an ``enum``, by not emitting a redundant
  file path for anonymous enums in the diagnostic, and by fixing a typo in the
  diagnostic.

- Improved :doc:`readability-identifier-naming
  <clang-tidy/checks/readability/identifier-naming>` check to
  validate ``namespace`` aliases.

- Improved :doc:`readability-implicit-bool-conversion
  <clang-tidy/checks/readability/implicit-bool-conversion>` check
  by adding the option `UseUpperCaseLiteralSuffix` to select the
  case of the literal suffix in fixes and fixing false positive for implicit
  conversion of comparison result in C23.

- Improved :doc:`readability-redundant-casting
  <clang-tidy/checks/readability/redundant-casting>` check
  by addressing a false positive in aggregate initialization through
  parenthesized list.

- Improved :doc:`readability-redundant-smartptr-get
  <clang-tidy/checks/readability/redundant-smartptr-get>` check to
  remove ``->``, when redundant ``get()`` is removed.

- Improved :doc:`readability-use-std-min-max
  <clang-tidy/checks/readability/use-std-min-max>` check to use correct template
  type in ``std::min`` and ``std::max`` when operand is integer literal.

Removed checks
^^^^^^^^^^^^^^

Miscellaneous
^^^^^^^^^^^^^

- The :doc:`bugprone-narrowing-conversions <clang-tidy/checks/bugprone/narrowing-conversions>`
  check is no longer an alias of :doc:`cppcoreguidelines-narrowing-conversions
  <clang-tidy/checks/cppcoreguidelines/narrowing-conversions>`. Instead,
  :doc:`cppcoreguidelines-narrowing-conversions
  <clang-tidy/checks/cppcoreguidelines/narrowing-conversions>` is now an alias
  of :doc:`bugprone-narrowing-conversions <clang-tidy/checks/bugprone/narrowing-conversions>`.

Improvements to include-fixer
-----------------------------

The improvements are...

Improvements to clang-include-fixer
-----------------------------------

The improvements are...

Improvements to modularize
--------------------------

The improvements are...

Improvements to pp-trace
------------------------

Clang-tidy Visual Studio plugin
-------------------------------
