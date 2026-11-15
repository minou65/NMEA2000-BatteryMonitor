Import("env")

import os
from pathlib import Path
import shutil
import tempfile


def copy_bin_and_commit_version(source, target, env):
    bin_file = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    project_name = Path(env.subst("$PROJECT_DIR")).name
    output_dir = os.environ.get("PLATFORMIO_BIN_DIR", "").strip()
    version_header = Path(env["VERSION_HEADER_SOURCE"])
    candidate_header = Path(env["VERSION_HEADER_CANDIDATE"])

    if not bin_file.is_file():
        raise FileNotFoundError("Firmware binary was not produced: {}".format(bin_file))
    if not candidate_header.is_file():
        raise FileNotFoundError(
            "Candidate version header was not produced: {}".format(candidate_header)
        )

    if output_dir:
        destination_dir = Path(
            os.path.expandvars(os.path.expanduser(output_dir))
        )
        destination = destination_dir / "{}_{}.bin".format(
            env.subst("$BOARD"), project_name
        )
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(bin_file), str(destination))
        print("Copied firmware to {}".format(destination))
    else:
        print("PLATFORMIO_BIN_DIR is unset; skipping the optional firmware copy.")

    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=str(version_header.parent),
            delete=False,
        ) as temporary_file:
            temporary_path = Path(temporary_file.name)
            temporary_file.write(candidate_header.read_text(encoding="utf-8"))
        os.replace(str(temporary_path), str(version_header))
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_bin_and_commit_version)