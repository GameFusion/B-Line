"""Publish an uploaded Windows ZIP through the production SyncPipeline publisher.

Run with the existing SyncPipeline virtualenv/configuration. Retains the current
release's other platform artifacts and visuals. No release notifications.
"""
import argparse
import hashlib
import importlib.util
import json
import subprocess
import sys
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path

import requests


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.hrefs = []

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            self.hrefs.extend(value for key, value in attrs if key == "href" and value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--syncpipeline-root", type=Path, required=True)
    parser.add_argument("--expected-release-id", type=int, required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    receipt = json.loads(args.receipt.read_text(encoding="utf-8-sig"))
    root = args.syncpipeline_root.resolve()
    sys.path.insert(0, str(root))
    spec = importlib.util.spec_from_file_location("tool_publisher", root / "scripts/publish_tool_release.py")
    publisher = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(publisher)
    settings = publisher.load_settings()
    slug = "b-line-boarder"
    version = receipt["Version"]
    url = receipt["URL"]
    expected_url = "https://fusion.ams3.digitaloceanspaces.com/Applications/B-Line/" + receipt["File"]
    if receipt["Name"] != "B-Line" or receipt["Platform"] != "Windows" or url != expected_url:
        raise RuntimeError("Unexpected release product or destination")

    def snapshot():
        with publisher.target_connection(settings) as db, db.cursor() as cursor:
            cursor.execute("""SELECT tr.*,ch.code AS channel_code,ch.label AS channel_label,
                ch.description AS channel_description FROM tool_releases tr
                JOIN tool_release_channels ch ON ch.id=tr.channel_id
                WHERE tr.tool_slug=%s AND tr.status='published'
                ORDER BY COALESCE(tr.published_at,tr.created_at) DESC,tr.id DESC LIMIT 1""", (slug,))
            release = cursor.fetchone()
            if not release:
                raise RuntimeError("Current B-Line Tools release not found")
            cursor.execute("SELECT * FROM tool_release_artifacts WHERE tool_release_id=%s ORDER BY sort_order,id", (release["id"],))
            artifacts = cursor.fetchall()
            cursor.execute("SELECT * FROM tool_release_visuals WHERE tool_release_id=%s ORDER BY sort_order,id", (release["id"],))
            visuals = cursor.fetchall()
            return release, artifacts, visuals

    previous, artifacts, visuals = snapshot()
    if previous["id"] != args.expected_release_id:
        raise RuntimeError("The active Tools release changed; inspect it before publishing")
    if not all(a["is_public"] for a in artifacts):
        raise RuntimeError("Publisher would expose a non-public artifact")
    windows = [a for a in artifacts if a["platform"].lower() == "windows"]
    if len(windows) != 1:
        raise RuntimeError("Expected exactly one existing Windows artifact")

    with publisher.target_connection(settings) as db, db.cursor() as cursor:
        cursor.execute("SELECT id FROM tool_releases WHERE tool_slug=%s AND version_label=%s", (slug, version))
        if cursor.fetchone():
            raise RuntimeError("This Tools version already exists; inspect instead of duplicating artifacts")

    fields = ("platform", "label", "architecture", "package_format", "file_name", "file_size_bytes",
              "checksum_sha256", "storage_url", "storage_key", "content_type", "is_primary", "sort_order", "metadata")
    new_artifacts = [{k: a[k] for k in fields} for a in artifacts]
    windows_new = next(a for a in new_artifacts if a["platform"].lower() == "windows")
    windows_new.update(label=f"B-Line {version} Windows Installer ZIP", architecture="x64", package_format="zip",
        file_name=receipt["File"], file_size_bytes=receipt["Size"], checksum_sha256=receipt["SHA256"],
        storage_url=url, storage_key="Applications/B-Line/" + receipt["File"], content_type="application/zip",
        metadata={**windows_new["metadata"], "availability_status": "available", "cta_label": "Download Windows Installer",
                  "availability_note": f"Optimized Windows x64 release {version}; fixes the scene-collapse freeze.",
                  "version": version, "signed": False, "release_date": datetime.now(timezone.utc).date().isoformat()})
    visual_fields = ("visual_kind", "title", "caption", "storage_url", "thumbnail_url", "sort_order", "metadata")
    manifest = {"tool_slug": slug,
        "channel": {"code": previous["channel_code"], "label": previous["channel_label"], "description": previous["channel_description"]},
        "release": {"version_label": version, "release_name": f"B-Line {version} - Windows update",
            "summary": "Optimized Windows release fixing the freeze after collapsing a scene and selecting a later shot.",
            "release_notes_markdown": "## Windows update\n\nFixes an accessibility traversal loop after scene collapse. "
                "Built with MSVC Release /O2 and /OPT:REF /OPT:ICF. Includes Qt, MSVC and FFmpeg runtimes. "
                "Uninstall an older installation before installing into the same directory. "
                "The Windows build is unsigned. Existing macOS and Linux offerings are retained.",
            "support_contact_url": previous["support_contact_url"],
            "metadata": {**previous["metadata"], "windows_version": version, "previous_tool_release_id": previous["id"], "legacy_deploy_id": receipt["id"]}},
        "artifacts": new_artifacts, "visuals": [{k: v[k] for k in visual_fields} for v in visuals]}
    output = args.receipt.resolve().parent
    backup_path = output / "syncpipeline-before.json"
    if backup_path.exists():
        raise RuntimeError("Existing backup found; inspect the prior attempt before retrying")
    backup_path.write_text(json.dumps({"release": previous, "artifacts": artifacts, "visuals": visuals}, default=str, indent=2), encoding="utf-8")
    manifest_path = output / "syncpipeline-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    command = [sys.executable, str(root / "scripts/publish_tool_release.py"), "--manifest", str(manifest_path), "--created-by", "admin"]
    subprocess.run(command, cwd=root, check=True)  # Official publisher's dry run.
    if not args.apply:
        return

    digest = hashlib.sha256()
    size = 0
    with requests.get(url, stream=True, timeout=(10, 60)) as response:
        response.raise_for_status()
        for chunk in response.iter_content(1024 * 1024):
            digest.update(chunk)
            size += len(chunk)
    if size != receipt["Size"] or digest.hexdigest() != receipt["SHA256"]:
        raise RuntimeError("Public ZIP checksum mismatch; Tools has not been updated")
    if snapshot()[0]["id"] != previous["id"]:
        raise RuntimeError("Another Tools release was published during verification")
    subprocess.run(command + ["--apply"], cwd=root, check=True)
    current, published_artifacts, published_visuals = snapshot()
    if current["version_label"] != version:
        raise RuntimeError("New release is not the current Tools release")
    for wanted in new_artifacts:
        actual = [a for a in published_artifacts if a["platform"] == wanted["platform"]]
        if len(actual) != 1 or any(actual[0][key] != wanted[key] for key in fields):
            raise RuntimeError("Published platform does not match the manifest")
    if [{k: v[k] for k in visual_fields} for v in published_visuals] != manifest["visuals"]:
        raise RuntimeError("Release visuals were not preserved")

    from app import app
    from flask import render_template
    from syncpipeline.views import _tools_page_model, _tool_detail_model
    pages = []
    for route, template in [("/tools", "tools.html"), ("/tools/" + slug, "tool_detail.html")]:
        with app.test_request_context(route, base_url="https://syncpipeline.com"):
            model = {"tools": _tools_page_model()} if route == "/tools" else {"tool": _tool_detail_model(slug)}
            links = Links()
            links.feed(render_template(template, **model))
            downloads = [href for href in links.hrefs if "B-Line-Setup-" in href]
            if not downloads or set(downloads) != {url}:
                raise RuntimeError("Production page renders an unexpected Windows download")
            pages.append({"page": route, "windows_download": url})
    with publisher.target_connection(settings) as db, db.cursor() as cursor:
        cursor.execute("SELECT id FROM tool_release_publish_jobs WHERE tool_slug=%s AND version_label=%s AND status='published' ORDER BY id DESC LIMIT 1", (slug, version))
        publish_job = cursor.fetchone()
    result = {"status": "published", "version": version, "toolReleaseId": current["id"],
        "windowsArtifactId": next(a["id"] for a in published_artifacts if a["platform"].lower() == "windows"),
        "publishJobId": publish_job["id"],
        "previousToolReleaseId": previous["id"], "url": url, "sha256": receipt["SHA256"], "bytes": size,
        "otherPlatformsPreserved": True, "visualsPreserved": True, "productionRenderedPages": pages,
        "verification": "production application and live database; public ZIP downloaded and SHA256 verified",
        "authenticatedHttpVerification": False, "publishedAt": datetime.now(timezone.utc).isoformat()}
    (output / "syncpipeline-published-release.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
