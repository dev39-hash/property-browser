# Documentation for developers of qpb

This folder is for people who work on qpb itself. Documentation for people who use qpb is in the component folder
and travels with it: start at [`qpb/README.md`](../qpb/README.md).

## Design

- [design/architecture.md](design/architecture.md): the two layers, the main classes, the data flow, ownership and
  the architecture constraints R1-R6.
- [design/public-headers.md](design/public-headers.md): how public headers are designed, how an API is added, and the
  checks that guard compatibility within 1.x.
- [design/component-cmake.md](design/component-cmake.md): the rules that keep `qpb/` from polluting the project that
  embeds it.

## Development

- [development/building-and-testing.md](development/building-and-testing.md): configure, build and test; the test
  layers, repository checks, the trial application, translations and the Qt Creator projects.
- [development/conventions.md](development/conventions.md): language level, naming, comments, encoding, language of
  the text, commit messages.
- [development/releasing.md](development/releasing.md): how a release is prepared, verified, built, tagged and handed
  over.
