#!/usr/bin/env python3
"""Public API snapshot for qpb (docs/SPEC.md §9.5, PLAN M4.8).

Extracts the public declarations of every header under qpb/include/qpb into a
normalized, sorted list: one line per declaration, prefixed with the header and
the enclosing scope. Private sections, function bodies and comments are
ignored; macros defined by the headers are included.

  api_snapshot.py --include qpb/include --write snapshot.txt
  api_snapshot.py --include qpb/include --check snapshot.txt

--check fails when a line of the snapshot is missing from the current headers:
something public was removed or its declaration changed (a breaking change,
docs/SPEC.md §9.2). New lines are reported but allowed (additive changes).
This is a textual check: it catches accidental removals and signature changes,
not every possible source incompatibility.
"""

import argparse
import pathlib
import re
import sys


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def normalize(statement):
    statement = re.sub(r"\s+", " ", statement).strip()
    statement = re.sub(r"\s*([(),<>*&=:\[\]])\s*", r"\1", statement)
    return statement.replace(":: ", "::")


def looks_like_function(head):
    return "(" in head and not re.match(r"^(class|struct|namespace|enum|union)\b", head)


def declarations(header_text):
    """Yields (scope, declaration) pairs for the public parts of a header."""
    lines = []
    for line in header_text.splitlines():
        stripped = line.strip()
        # Qt macros without a trailing semicolon would glue onto the next
        # declaration; they carry no API of their own here.
        if re.fullmatch(r"(QT_BEGIN_NAMESPACE|QT_END_NAMESPACE|Q_OBJECT|Q_GADGET|"
                        r"(Q_ENUM|Q_PROPERTY|Q_DECLARE_FLAGS|Q_DECLARE_METATYPE|"
                        r"Q_DECLARE_OPERATORS_FOR_FLAGS)\(.*\))", stripped):
            continue
        if stripped.startswith("#"):
            match = re.match(r"#\s*define\s+(QPB_\w+(\([^)]*\))?)", stripped)
            if match and not match.group(1).endswith("_H"):
                yield ("macro", normalize(match.group(1)))
            continue
        lines.append(line)
    text = strip_comments("\n".join(lines))

    scopes = []  # (name, kind, access)
    current = ""
    i = 0
    while i < len(text):
        char = text[i]
        if char == "{":
            head = re.sub(r"^template<[^>]*>", "", normalize(current))
            current = ""
            kind_match = re.match(r"^(?:inline )?(class|struct|namespace|enum class|enum|union)\b\s*(.*)$", head)
            if not kind_match:
                # Function body or brace initializer (e.g. a constant): record the
                # declaration and skip to the matching brace.
                depth, start = 1, i + 1
                i += 1
                while i < len(text) and depth:
                    depth += {"{": 1, "}": -1}.get(text[i], 0)
                    i += 1
                if head and not head.startswith("return") and visible(scopes):
                    if "(" in head:
                        yield (scope_name(scopes), head)  # function: the body is not API
                    else:
                        # Constant: its value is part of the API.
                        yield (scope_name(scopes), head + "{" + normalize(text[start : i - 1]) + "}")
                continue
            kind = kind_match.group(1)
            words = [w for w in re.split(r"[\s:]+", kind_match.group(2))
                     if w and not re.fullmatch(r"QPB_\w+_EXPORT|final", w)]
            name = words[0] if words else ""
            if kind in ("class", "struct", "union", "enum", "enum class") and visible(scopes):
                yield (scope_name(scopes), head)
            if kind in ("enum", "enum class"):
                depth, start = 1, i + 1
                i += 1
                while i < len(text) and depth:
                    depth += {"{": 1, "}": -1}.get(text[i], 0)
                    i += 1
                body = text[start : i - 1]
                if visible(scopes):
                    for enumerator in body.split(","):
                        enumerator = normalize(enumerator)
                        if enumerator:
                            yield (scope_name(scopes) + "::" + name, enumerator)
                continue
            access = "private" if kind == "class" else "public"
            scopes.append((re.sub(r"\W.*$", "", name) if name else "", kind, access))
        elif char == "}":
            current = ""
            if scopes:
                scopes.pop()
        elif char == ";":
            statement = normalize(current)
            current = ""
            if statement and visible(scopes):
                yield (scope_name(scopes), statement)
        elif char == ":" and scopes and re.fullmatch(r"\s*(public|protected|private)\s*",
                                                     current + ""):
            access = current.strip()
            name, kind, _ = scopes[-1]
            scopes[-1] = (name, kind, access)
            current = ""
        else:
            current += char
            match = re.search(r"(public|protected|private)\s*:(?!:)\s*$", current)
            if match and scopes and scopes[-1][1] in ("class", "struct"):
                name, kind, _ = scopes[-1]
                scopes[-1] = (name, kind, match.group(1))
                current = ""
        i += 1


def visible(scopes):
    for name, kind, access in scopes:
        if kind == "namespace" and name == "detail":
            return False
        if kind in ("class", "struct") and access == "private":
            return False
    return True


def scope_name(scopes):
    parts = [name + ("" if access == "public" or kind == "namespace" else f"[{access}]")
             for name, kind, access in scopes if name]
    return "::".join(parts) or "::"


def snapshot(include_dir):
    root = pathlib.Path(include_dir)
    result = set()
    for header in sorted(root.rglob("*.h")):
        relative = header.relative_to(root).as_posix()
        for scope, declaration in declarations(header.read_text(encoding="utf-8")):
            if declaration.startswith(("Q_OBJECT", "Q_ENUM", "Q_DECLARE", "Q_PROPERTY", "QT_")):
                continue
            result.add(f"{relative} | {scope} | {declaration}")
    return sorted(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--include", required=True, help="the qpb/include directory")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", metavar="FILE")
    group.add_argument("--check", metavar="FILE")
    args = parser.parse_args()

    current = snapshot(args.include)
    if args.write:
        pathlib.Path(args.write).write_text("\n".join(current) + "\n", encoding="utf-8")
        print(f"Wrote {len(current)} declarations to {args.write}")
        return 0

    baseline = [line for line in pathlib.Path(args.check).read_text(encoding="utf-8").splitlines() if line]
    missing = sorted(set(baseline) - set(current))
    added = sorted(set(current) - set(baseline))
    for line in added:
        print(f"added:   {line}")
    for line in missing:
        print(f"REMOVED: {line}")
    if missing:
        print(f"{len(missing)} public declaration(s) removed or changed: breaking change "
              "(docs/SPEC.md §9.2). If intended before 1.0, regenerate the snapshot with --write.")
        return 1
    print(f"API snapshot OK: {len(baseline)} declarations kept, {len(added)} added")
    return 0


if __name__ == "__main__":
    sys.exit(main())
