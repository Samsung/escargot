#!/usr/bin/env python3
# Copyright (c) 2026-present Samsung Electronics Co., Ltd
# SPDX-License-Identifier: LGPL-2.1-or-later

"""Merge successful Android-Release artifacts into a Maven branch checkout."""

import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET
import zipfile


GROUP = "com.samsung.lwe.escargot"
GROUP_PATH = Path(*GROUP.split("."))
HASHES = ("md5", "sha1", "sha256", "sha512")
VERSION = re.compile(r"(\d+)\.(\d+)\.(\d+)\.(\d{8})\.(\d+)\.([0-9a-f]{7,40})")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def version_key(version):
    match = VERSION.fullmatch(version)
    require(match is not None, f"Unexpected Maven version: {version}")
    return tuple(map(int, match.groups()[:5])) + (match[6],)


def digest(data, algorithm):
    return hashlib.new(algorithm, data).hexdigest()


def checked_payload(path):
    data = path.read_bytes()
    for algorithm in HASHES:
        checksum = Path(f"{path}.{algorithm}").read_text().strip()
        require(checksum == digest(data, algorithm), f"Checksum mismatch: {path}.{algorithm}")
    return data


def unique_file(root, name):
    matches = list(root.rglob(name))
    require(len(matches) == 1, f"Expected one {name} under {root}, found {len(matches)}")
    return matches[0]


def host_jar(path):
    with zipfile.ZipFile(path) as archive:
        require("libescargot-jni.so" in archive.namelist(), f"Missing native library in {path}")
        require("com/samsung/lwe/escargot/Escargot.class" in archive.namelist(),
                f"Missing Escargot classes in {path}")
        require(archive.testzip() is None, f"Corrupt JAR: {path}")
    return path.read_bytes()


def xml_bytes(root):
    ET.indent(root, space="  ")
    return ET.tostring(root, encoding="UTF-8", xml_declaration=True) + b"\n"


def host_pom(artifact, version):
    root = ET.Element("project", {"xmlns": "http://maven.apache.org/POM/4.0.0"})
    for name, value in (("modelVersion", "4.0.0"), ("groupId", GROUP),
                        ("artifactId", artifact), ("version", version), ("packaging", "jar")):
        ET.SubElement(root, name).text = value
    return xml_bytes(root)


def metadata(path, artifact, versions):
    versions = sorted(versions, key=version_key)
    if path.exists():
        old = ET.parse(path).getroot()
        require(old.findtext("groupId") == GROUP and old.findtext("artifactId") == artifact,
                f"Wrong coordinates in {path}")
        if (old.findall("versioning/versions/version")
                and [v.text for v in old.findall("versioning/versions/version")] == versions
                and old.findtext("versioning/latest") == versions[-1]
                and old.findtext("versioning/release") == versions[-1]):
            return path.read_bytes()  # Preserve lastUpdated on an identical rerun.
    root = ET.Element("metadata")
    ET.SubElement(root, "groupId").text = GROUP
    ET.SubElement(root, "artifactId").text = artifact
    versioning = ET.SubElement(root, "versioning")
    ET.SubElement(versioning, "latest").text = versions[-1]
    ET.SubElement(versioning, "release").text = versions[-1]
    entries = ET.SubElement(versioning, "versions")
    for version in versions:
        ET.SubElement(entries, "version").text = version
    ET.SubElement(versioning, "lastUpdated").text = datetime.datetime.now(
        datetime.timezone.utc).strftime("%Y%m%d%H%M%S")
    return xml_bytes(root)


def prepare(repository, artifacts, tag, sha):
    require((repository / ".git").exists(), "Expected a Maven branch Git checkout")
    require(re.fullmatch(r"v\d+\.\d+\.\d+", tag), "Expected a release tag")
    require(re.fullmatch(r"[0-9a-f]{40}", sha), "Expected a full source commit SHA")
    android = artifacts / "maven-publish-android" / GROUP_PATH / "escargot-android"
    source_metadata = ET.fromstring(checked_payload(android / "maven-metadata.xml"))
    versions = [v.text for v in source_metadata.findall("versioning/versions/version")]
    require(len(versions) == 1, "Expected exactly one version in the Android publish output")
    version = versions[0]
    version_key(version)
    match = VERSION.fullmatch(version)
    require(".".join(match.groups()[:3]) == tag[1:] and sha.startswith(match[6]),
            "Maven version does not match the release tag and source commit")
    datetime.datetime.strptime(match[4], "%Y%m%d")
    require(source_metadata.findtext("groupId") == GROUP
            and source_metadata.findtext("artifactId") == "escargot-android",
            "Wrong Android publication coordinates")

    stem = f"escargot-android-{version}"
    android_payloads = {}
    for suffix in (".aar", ".pom", ".module", "-sources.jar", "-javadoc.jar"):
        name = stem + suffix
        android_payloads[name] = checked_payload(android / version / name)
    pom = ET.fromstring(android_payloads[stem + ".pom"])
    for name, expected in (("groupId", GROUP), ("artifactId", "escargot-android"),
                           ("version", version), ("packaging", "aar")):
        require(pom.findtext(f"{{*}}{name}") == expected, f"Wrong {name} in Android POM")
    module = json.loads(android_payloads[stem + ".module"])
    require(all(module["component"].get(k) == v for k, v in
                (("group", GROUP), ("module", "escargot-android"), ("version", version))),
            "Wrong Gradle module coordinates")
    for variant in module["variants"]:
        for entry in variant.get("files", []):
            require(entry["url"] in android_payloads, "Unexpected Gradle module file URL")
            data = android_payloads[entry["url"]]
            require(entry["size"] == len(data), "Gradle module file size mismatch")
            require(all(entry[a] == digest(data, a) for a in HASHES),
                    "Gradle module checksum mismatch")

    packages = {"android": android_payloads}
    for platform in ("ubuntu", "mac"):
        artifact = f"escargot-{platform}"
        jar = unique_file(artifacts / f"build-artifact-{platform}", artifact + ".jar")
        packages[platform] = {
            f"{artifact}-{version}.jar": host_jar(jar),
            f"{artifact}-{version}.pom": host_pom(artifact, version),
        }

    # Plan and check all three packages before modifying the checkout.
    writes = {}
    mutable = set()

    def add(path, data, is_metadata=False):
        writes[path] = data
        for algorithm in HASHES:
            writes[Path(f"{path}.{algorithm}")] = digest(data, algorithm).encode() + b"\n"
        if is_metadata:
            mutable.update([path] + [Path(f"{path}.{a}") for a in HASHES])

    for platform, payloads in packages.items():
        artifact = f"escargot-{platform}"
        destination = repository / platform / "releases" / GROUP_PATH / artifact
        for name, data in payloads.items():
            add(destination / version / name, data)
        # Gradle's fresh output lists only this release. Merge it with the
        # branch's history, including the legacy Maven install-file metadata.
        known = {version}
        paths = [destination / "maven-metadata.xml"]
        local_metadata = destination / "maven-metadata-local.xml"
        if local_metadata.exists():
            paths.append(local_metadata)
        for path in paths:
            if path.exists():
                known.update(v.text for v in ET.parse(path).findall("versioning/versions/version"))
        for path in paths:
            add(path, metadata(path, artifact, known), is_metadata=True)

    for path, data in writes.items():
        current = repository
        for part in path.relative_to(repository).parts:
            current = current / part
            require(not current.is_symlink(), f"Refusing symlink in Maven destination: {current}")
        if path.exists() and path not in mutable:
            require(path.read_bytes() == data, f"Refusing to overwrite published artifact: {path}")
    for path, data in writes.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True, type=Path)
    parser.add_argument("--artifacts", required=True, type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--sha", required=True)
    args = parser.parse_args()
    print(prepare(args.repository.resolve(), args.artifacts.resolve(), args.tag, args.sha))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, ET.ParseError, KeyError, zipfile.BadZipFile) as error:
        sys.exit(str(error))
