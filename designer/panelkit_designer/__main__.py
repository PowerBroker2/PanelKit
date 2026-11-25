"""Command line entry point. With no arguments, open the graphical editor."""

import argparse
from pathlib import Path

from .export import export_project
from .project import load_project


def main(argv=None):
    parser = argparse.ArgumentParser(prog="panelkit-designer")
    commands = parser.add_subparsers(dest="command")
    gui = commands.add_parser("gui", help="Open the visual editor")
    gui.add_argument("project", nargs="?", type=Path)
    export = commands.add_parser("export", help="Generate C++ from a project file")
    export.add_argument("project", type=Path)
    export.add_argument("folder", type=Path)
    export.add_argument("--integrate", action="store_true",
                        help="Write just the generated files for an existing sketch")
    args = parser.parse_args(argv)
    if args.command == "export":
        try:
            for path in export_project(load_project(args.project), args.folder,
                                       create_sketch=not args.integrate):
                print(path)
        except (OSError, ValueError, TypeError) as error:
            parser.exit(1, f"Export failed. {error}\n")
        return 0
    try:
        from .gui import run
        run(getattr(args, "project", None))
    except (OSError, ValueError, TypeError) as error:
        parser.exit(1, f"Could not open PanelKit Designer. {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

