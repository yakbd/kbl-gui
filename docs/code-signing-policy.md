# Code signing policy

**Status: planned.** The project has applied to SignPath Foundation; signing has not started yet. Every release
so far ships its files unsigned. This page describes how signing will work once the project has been accepted.
Releases can already be checked with their `SHA256SUMS` file and build provenance attestation (see the release
notes and [`VERIFIED.md`](../VERIFIED.md)).

Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by
[SignPath Foundation](https://signpath.org).

## What is signed

Only Windows files that this project builds from its own source:

- `yakbd-gui-setup.exe`, the Windows installer of an editor release (the yakbd tool's own installer carrying the
  editor), published in the [releases](https://github.com/yakbd/yakbd-gui/releases/latest) of `yakbd/yakbd-gui`;
- inside it, the project's own programs: `yakbd-gui.exe` (the editor, from this repository) and `yakbd.exe` (the
  command-line tool, from [`yakbd/yakbd-cli`](https://github.com/yakbd/yakbd-cli), which has its own
  [code signing policy](https://github.com/yakbd/yakbd-cli/blob/main/docs/code-signing-policy.md)).

The certificate is SignPath Foundation's, so Windows shows "SignPath Foundation" as the publisher.

Not signed:

- Qt's libraries and plugins, the Visual C++ runtime and any other third-party file the installer carries: they
  are other projects' files and are shipped as their makers published them;
- the macOS `.dmg` and the Linux AppImage (for now);
- layout installers (`<dll name>-setup.exe`) that the yakbd tool writes for layout authors: authors build them on
  their own machines, and they are never signed with this project's certificate.

## How a signed release is made

- **Built from this repository's source only**, by the `Packages` workflow (`.github/workflows/release.yml`) on
  GitHub-hosted runners, with the command-line tool of a tagged yakbd-cli release whose files the workflow checks
  against that release's `SHA256SUMS` and build provenance attestation. Nobody builds a release on their own
  machine.
- **Started by hand.** The workflow runs only when a maintainer starts it for a version; it has no automatic
  trigger. Every build of a release carries the same product name ("YAKBD Editor") and version.
- **Signed only after approval.** The signing step sends the built files to SignPath: first `yakbd-gui.exe`, before
  the installer is assembled, then `yakbd-gui-setup.exe`. An approver checks that the request comes from this
  repository's Packages workflow for the intended version, and approves it in SignPath. Every release is approved
  by hand.
- **Published unchanged.** The release is published with its assets and tag frozen (an immutable GitHub
  release), its `SHA256SUMS` file and a build provenance attestation for every file.

## Team roles

- **Authors** (may change the source without further review): the members of the
  [yakbd organisation](https://github.com/orgs/yakbd/people) with write access to `yakbd/yakbd-gui`.
- **Reviewers**: every change from someone outside the authors, such as a pull request, is reviewed by an author
  before it is merged.
- **Approvers** (approve each signing request): the [owners of the yakbd
  organisation](https://github.com/orgs/yakbd/people?query=role%3Aowner).

Every team member must use multi-factor authentication for GitHub and for SignPath.

## Privacy

This program will not transfer any information to other networked systems unless specifically requested by the
user or the person installing or operating it.

The editor's own code contains no network code: no Qt Network classes, sockets or HTTP requests, and it loads its
user interface only from its own resources. The only links it opens are those of its Help menu and About box, in
the user's web browser, when the user chooses them. Qt's network library is in the package because Qt's QML
engine links it; the editor does not use it. The yakbd tool it runs has no network code either.

## Reporting

If you think a file signed under this policy is not what this page says, or was signed by mistake, open an issue
in [`yakbd/yakbd-gui`](https://github.com/yakbd/yakbd-gui/issues), or write to SignPath Foundation
([signpath.org](https://signpath.org)).
