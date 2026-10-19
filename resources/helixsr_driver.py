# Used by WindowManager: puts HelixSR into one game folder (or takes it out again) by calling the
# functions of the HelixSR setup it ships with. HelixSR's own files are not changed.
#
#   python helixsr_driver.py <HelixSR folder> install|remove <game folder>
#
# Exit code 2 (with a "driver-incompatible:" line) means HelixSR's installer no longer has the
# functions this script uses, i.e. a newer HelixSR changed it; WindowManager then switches installing off.
import sys
from pathlib import Path

INCOMPATIBLE = 2


def main():
    pkg = Path(sys.argv[1]).resolve()
    command = sys.argv[2]
    folder = sys.argv[3]

    sys.path.insert(0, str(pkg / 'setup'))
    import helixsr_install as helix

    games = helix.games_from([folder])
    if not games:
        raise SystemExit('no FSR, DLSS or XeSS found in ' + folder)
    game = games[0]

    if command == 'install':
        if not game.fsr_targets:
            raise SystemExit('no FSR 3.1 DLL found: the FSR drop-in cannot be used for this game')
        game.route = 'direct'
        names, ok = helix.install_list([game], pkg, False)   # False: no frame generation (HelixSR Scaler only)
        if not ok:
            raise SystemExit(1)
    elif command == 'remove':
        helix.uninstall_safe(game)
    else:
        raise SystemExit('unknown command: ' + command)


try:
    main()
except (ImportError, AttributeError, TypeError, NameError) as error:
    # These come from calling HelixSR's functions with the wrong shape, not from the game's files.
    print('driver-incompatible: %s: %s' % (type(error).__name__, error), flush=True)
    raise SystemExit(INCOMPATIBLE)
