# PlatformIO post script: passes the firmware version from the VERSION file to
# the source files in src/ as FW_VERSION (AGENTS.md, Firmware versioning;
# PROJECT.md D-035).
#
# The version is kept out of platformio.ini on purpose: PlatformIO wipes the
# whole build folder whenever platformio.ini changes, so a version bump there
# rebuilt every library (about 9 minutes per board). With this script a bump
# recompiles only src/.
import re
from pathlib import Path

Import("env", "projenv")  # noqa: F821 (provided by PlatformIO)

version_file = Path(env.subst("$PROJECT_DIR")) / "VERSION"  # noqa: F821
version = version_file.read_text(encoding="utf-8").strip()
if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
    raise SystemExit(f"VERSION must be MAJOR.MINOR.PATCH without leading zeros, found {version!r}")

projenv.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])  # noqa: F821
print(f"FW_VERSION {version}")
