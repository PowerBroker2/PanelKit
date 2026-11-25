"""Command line entry point. With no arguments, open the graphical editor."""

import argparse
from pathlib import Path

from .export import export_project
from .project import load_project
from .menu import export_menu_page
from .menu_view import export_menu_view


def main(argv=None):
    parser = argparse.ArgumentParser(prog="panelkit-studio")
    commands = parser.add_subparsers(dest="command")
    gui = commands.add_parser("gui", help="Open the visual editor")
    gui.add_argument("project", nargs="?", type=Path)
    export = commands.add_parser("export", help="Generate C++ from a project file")
    export.add_argument("project", type=Path)
    export.add_argument("folder", type=Path)
    export.add_argument("--integrate", action="store_true",
                        help="Write just the generated files for an existing sketch")
    menu = commands.add_parser("export-menu", help="Export a Studio screen or panel as one SD menu page")
    menu.add_argument("project", type=Path)
    menu.add_argument("file", type=Path)
    menu.add_argument("--section", default="", help="ID of a container to export")
    menu.add_argument("--title", required=True)
    view = commands.add_parser("export-menu-view", help="Export a full scene plus an SD view selector")
    view.add_argument("project", type=Path)
    view.add_argument("folder", type=Path)
    view.add_argument("--name", required=True)
    view.add_argument("--title", required=True)
    args = parser.parse_args(argv)
    if args.command == "export-menu-view":
        try:
            for path in export_menu_view(load_project(args.project), args.folder, args.name, args.title):
                print(path)
        except (OSError, ValueError, TypeError) as error:
            parser.exit(1, f"Menu view export failed. {error}\n")
        return 0
    if args.command == "export-menu":
        try:
            print(export_menu_page(load_project(args.project), args.file, args.title, args.section))
        except (OSError, ValueError, TypeError) as error:
            parser.exit(1, f"Menu export failed. {error}\n")
        return 0
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
        parser.exit(1, f"Could not open PanelKit Studio. {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

