#!/usr/bin/env python3
"""Updates qpb's translations (docs/SPEC.md §4.10, PLAN M12.3).

  update_translations.py --qt-bin C:/Qt/6.11.1/msvc2022_64/bin
  update_translations.py --qt-bin <dir> --check

Without --check:
1. lupdate brings qpb/translations/qpb_<lang>.ts up to date with the
   translatable texts of qpb/src and qpb/include: new texts are added as
   unfinished, texts that left the sources are dropped. qpb_en.ts holds the
   plural forms only (English is the source language).
2. lrelease writes qpb_<lang>.qm next to each .ts. The .qm files are committed
   and embedded in qpb::core (D55), so consumers need no Qt Linguist tools.
   Translate the unfinished texts (Qt Linguist or an editor) and run it again.

--check changes nothing: it fails when a .ts file misses a text of the sources,
keeps one that left them, or has an unfinished translation (the CTest
translations_up_to_date). That the .qm files match the .ts files is checked by
tests/core/tst_translations.
"""

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ElementTree

LANGUAGES = ("en", "ja", "vi")

ROOT = pathlib.Path(__file__).resolve().parent.parent
QPB = ROOT / "qpb"
TRANSLATIONS = QPB / "translations"


def tool(qt_bin, name):
    for candidate in (name, name + ".exe"):
        path = pathlib.Path(qt_bin) / candidate
        if path.is_file():
            return str(path)
    sys.exit(f"update_translations: {name} not found in {qt_bin}")


def run_lupdate(lupdate, language, ts_file):
    command = [lupdate, "-silent", "-locations", "none", "-no-obsolete",
               "-source-language", "en", "-target-language", language]
    if language == "en":
        command.append("-pluralonly")
    command += [str(QPB / "src"), str(QPB / "include"), "-ts", str(ts_file)]
    subprocess.run(command, check=True)


def messages(ts_file):
    """{(context, source): unfinished} for every message of a .ts file."""
    result = {}
    root = ElementTree.parse(ts_file).getroot()
    for context in root.iter("context"):
        name = context.findtext("name")
        for message in context.iter("message"):
            translation = message.find("translation")
            if translation is None:
                unfinished = True
            else:
                forms = [form.text for form in translation.iter("numerusform")]
                texts = forms if forms else [translation.text]
                unfinished = (translation.get("type") in ("unfinished", "vanished", "obsolete")
                              or not all(texts))
            result[(name, message.findtext("source"))] = unfinished
    return result


def check(lupdate):
    failures = []
    with tempfile.TemporaryDirectory() as temporary:
        for language in LANGUAGES:
            committed = TRANSLATIONS / f"qpb_{language}.ts"
            if not committed.is_file():
                failures.append(f"{committed.name}: missing")
                continue
            updated = pathlib.Path(temporary) / committed.name
            shutil.copyfile(committed, updated)
            run_lupdate(lupdate, language, updated)
            before, after = messages(committed), messages(updated)
            for key in sorted(set(after) - set(before)):
                failures.append(f"{committed.name}: text not in the file: {key[0]}: {key[1]!r}")
            for key in sorted(set(before) - set(after)):
                failures.append(f"{committed.name}: text no longer in the sources: {key[0]}: {key[1]!r}")
            for key, unfinished in sorted(before.items()):
                if unfinished and key in after:
                    failures.append(f"{committed.name}: unfinished: {key[0]}: {key[1]!r}")
    for failure in failures:
        print(failure)
    if failures:
        print("Translations out of date: run tools/update_translations.py, translate, commit "
              "the .ts and .qm files (docs/SPEC.md §4.10).")
        return 1
    print(f"Translations OK: {', '.join(LANGUAGES)}")
    return 0


def update(lupdate, lrelease):
    for language in LANGUAGES:
        ts_file = TRANSLATIONS / f"qpb_{language}.ts"
        run_lupdate(lupdate, language, ts_file)
        subprocess.run([lrelease, "-silent", str(ts_file), "-qm", str(ts_file.with_suffix(".qm"))],
                       check=True)
        unfinished = [key for key, value in messages(ts_file).items() if value]
        state = f"{len(unfinished)} unfinished" if unfinished else "complete"
        print(f"{ts_file.name}: {state}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--qt-bin", required=True, help="Qt's bin directory (lupdate, lrelease)")
    parser.add_argument("--check", action="store_true", help="check only, change nothing")
    args = parser.parse_args()
    lupdate = tool(args.qt_bin, "lupdate")
    if args.check:
        return check(lupdate)
    return update(lupdate, tool(args.qt_bin, "lrelease"))


if __name__ == "__main__":
    sys.exit(main())
