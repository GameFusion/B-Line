"""Upload a verified Windows installer and register it in the legacy Deploy feed.

Uses the existing GitExplorer publishing credentials without printing/copying them.
The SyncPipeline Tools PostgreSQL catalog requires a separate publication step.
No email, Slack, Teams, schema changes, or removal of previous releases.
"""
import argparse
import ast
import hashlib
import json
from datetime import datetime
from pathlib import Path
from urllib.parse import quote

import boto3
import mysql.connector
import requests
from botocore.config import Config
from botocore.exceptions import ClientError


def legacy_config(path, function_name):
    tree = ast.parse(path.read_text(encoding="utf-8-sig"))
    function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == function_name)
    values = {}
    for node in function.body:
        if isinstance(node, ast.Assign) and isinstance(node.value, ast.Constant):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    values[target.id] = node.value.value
    return function, values


def clients(helper_dir):
    function, values = legacy_config(helper_dir / "S3.py", "upload_file")
    call = next(n for n in ast.walk(function) if isinstance(n, ast.Call)
                and isinstance(n.func, ast.Attribute) and n.func.attr == "client")
    kwargs = {k.arg: values[k.value.id] if isinstance(k.value, ast.Name) else ast.literal_eval(k.value)
              for k in call.keywords}
    kwargs["verify"] = True
    kwargs["config"] = Config(connect_timeout=10, read_timeout=60, retries={"max_attempts": 2})
    bucket = values["space_name"]
    endpoint = kwargs["endpoint_url"]
    if bucket != "fusion" or endpoint != "https://ams3.digitaloceanspaces.com":
        raise RuntimeError("Publishing destination differs from the established B-Line release storage")
    storage = boto3.client("s3", **kwargs)
    _, values = legacy_config(helper_dir / "SQL.py", "connect")
    database = mysql.connector.connect(host=values["host"], user=values["user"],
        password=values["password"], database=values["DB"], connection_timeout=10)
    return storage, bucket, database


def digest(path, algorithm):
    with path.open("rb") as source:
        return hashlib.file_digest(source, algorithm).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("release_json", type=Path)
    parser.add_argument("--helper-dir", type=Path, default=Path(r"F:\GameSource\Applications\GitExplorer"))
    parser.add_argument("--publish", action="store_true", help="Upload, verify the public download, then insert the release record")
    args = parser.parse_args()
    release = json.loads(args.release_json.read_text(encoding="utf-8-sig"))
    archive = Path(release["archive"]).resolve(strict=True)
    sha256 = digest(archive, "sha256")
    if sha256 != release["sha256"]:
        raise RuntimeError("The archive has changed since packaging")
    key = "Applications/B-Line/" + archive.name
    url = "https://fusion.ams3.digitaloceanspaces.com/" + quote(key, safe="/")
    storage, bucket, database = clients(args.helper_dir)
    cursor = database.cursor(dictionary=True)
    try:
        storage.list_objects_v2(Bucket=bucket, Prefix="Applications/B-Line/", MaxKeys=1)
        cursor.execute("SELECT * FROM Deploy WHERE Name=%s AND Platform=%s ORDER BY id DESC LIMIT 10", ("B-Line", "Windows"))
        previous = cursor.fetchall()
        if not previous:
            raise RuntimeError("Existing B-Line Windows release not found")
        print(json.dumps({"credentials": "storage and database verified", "version": release["version"],
            "archive": archive.name, "bytes": archive.stat().st_size, "sha256": sha256,
            "url": url, "previousReleaseId": previous[0]["id"]}, indent=2))
        if not args.publish:
            return
        backup = args.release_json.parent / "previous-windows-releases.json"
        if not backup.exists():
            backup.write_text(json.dumps(previous, default=str, indent=2), encoding="utf-8")
        try:
            existing = storage.head_object(Bucket=bucket, Key=key)
        except ClientError as error:
            if error.response["Error"]["Code"] not in ("404", "NoSuchKey"):
                raise
            existing = None
        if existing:
            if existing.get("Metadata", {}).get("sha256") != sha256 or existing["ContentLength"] != archive.stat().st_size:
                raise RuntimeError("A different artifact already occupies this release key")
        else:
            storage.upload_file(str(archive), bucket, key, ExtraArgs={"ACL": "public-read",
                "ContentType": "application/zip", "ContentDisposition": f'attachment; filename="{archive.name}"',
                "Metadata": {"sha256": sha256, "version": release["version"]}})
            print("Archive uploaded")

        # Verify public bytes before recording the legacy deployment.
        downloaded = hashlib.sha256()
        size = 0
        with requests.get(url, stream=True, timeout=(10, 60)) as response:
            response.raise_for_status()
            for chunk in response.iter_content(1024 * 1024):
                downloaded.update(chunk)
                size += len(chunk)
        if size != archive.stat().st_size or downloaded.hexdigest() != sha256:
            raise RuntimeError("Public download checksum verification failed; release feed was not updated")
        print("Public download size and SHA256 verified")

        now = datetime.now()
        manifest = json.loads((Path(release["stagedBundle"]) / "build-manifest.json").read_text(encoding="utf-8-sig"))
        metadata = {"User": previous[0]["User"], "Name": "B-Line", "File": archive.name,
            "Status": previous[0]["Status"], "Task": previous[0]["Task"], "URL": url,
            "Size": size, "MD5": digest(archive, "md5"), "SHA256": sha256,
            "Date": now.strftime("%Y-%m-%d %H:%M"), "Platform": "Windows", "Version": release["version"],
            "ReleaseDate": now.strftime("%Y-%m-%d"), "CommitSha": manifest["sourceCommit"],
            "CommitResolutionMethod": "publish", "CommitGuessDistanceSeconds": 0}
        database.rollback()  # End the preflight read snapshot before checking concurrency.
        cursor.execute("SELECT id,SHA256 FROM Deploy WHERE Name=%s AND Platform=%s AND URL=%s ORDER BY id DESC LIMIT 1", ("B-Line", "Windows", url))
        published = cursor.fetchone()
        if published:
            if published["SHA256"] != sha256:
                raise RuntimeError("Existing release record has a different checksum")
            deploy_id = published["id"]
        else:
            cursor.execute("SELECT id FROM Deploy WHERE Name=%s AND Platform=%s ORDER BY id DESC LIMIT 1", ("B-Line", "Windows"))
            if cursor.fetchone()["id"] != previous[0]["id"]:
                raise RuntimeError("Another Windows release was published during upload; inspect it before continuing")
            columns = ",".join("`" + name + "`" for name in metadata)
            placeholders = ",".join(["%s"] * len(metadata))
            cursor.execute(f"INSERT INTO Deploy ({columns}) VALUES ({placeholders})", tuple(metadata.values()))
            deploy_id = cursor.lastrowid
            database.commit()
        cursor.execute("SELECT id,Name,Platform,Version,File,URL,SHA256,Size FROM Deploy WHERE Name=%s AND Platform=%s ORDER BY id DESC LIMIT 1", ("B-Line", "Windows"))
        confirmed = cursor.fetchone()
        if confirmed["id"] != deploy_id or confirmed["SHA256"] != sha256 or confirmed["URL"] != url:
            raise RuntimeError("Latest release record verification failed")
        receipt = {**confirmed, "publishedAt": now.isoformat(), "previousReleaseId": previous[0]["id"],
            "publicDownloadVerified": True, "signed": False,
            "publicationScope": "storage-and-legacy-deploy",
            "syncPipelineToolsStatus": "separate-publication-required"}
        (args.release_json.parent / "published-release.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
        print("STORAGE_AND_LEGACY_DEPLOY_PUBLISHED " + json.dumps(receipt))
        print("SyncPipeline Tools still requires its PostgreSQL catalog update and page verification.")
    finally:
        cursor.close()
        database.close()


if __name__ == "__main__":
    main()
