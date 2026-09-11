"""instrument - command-line client for the class-board instrument.

Mirrors the WebSocket protocol in firmware/PROTOCOL.md one-to-one:

    instrument status
    instrument send base led --args '{"r":0,"g":255,"b":0}'
    instrument stream base.counter --seconds 5 --csv out.csv
    instrument alarms list
    instrument alarms add --block b1 --key ai1 --op gt --threshold 9 --action relay:1:off
    instrument alarms remove 3

The board is found at instrument.local (mDNS), falling back to 192.168.4.1 (its own access point).
On a shared network pass --host instrument-XXXX.local (XXXX = the four hex digits in the
access-point name). Override with --host.
"""

from __future__ import annotations

import asyncio
import csv
import json
import socket
import sys
import time
from typing import Any

import typer
import websockets

app = typer.Typer(help=__doc__, no_args_is_help=True)
alarms_app = typer.Typer(help="Manage alarm rules on the board.")
app.add_typer(alarms_app, name="alarms")

DEFAULT_HOSTS = ("instrument.local", "192.168.4.1")


# --------------------------------------------------------------------------
# Connection
# --------------------------------------------------------------------------
def discover(host: str | None) -> str:
    """Return the first host that resolves (mDNS or the AP address)."""
    candidates = [host] if host else list(DEFAULT_HOSTS)
    for h in candidates:
        try:
            socket.getaddrinfo(h, 80, proto=socket.IPPROTO_TCP)
            return h
        except socket.gaierror:
            continue
    typer.echo(f"cannot resolve {' or '.join(candidates)}; use --host", err=True)
    raise typer.Exit(2)


class Client:
    """Tiny request/reply client. One WebSocket, ids matched to replies."""

    def __init__(self, host: str):
        self.url = f"ws://{host}/ws"
        self.ws = None
        self.next_id = 1

    async def __aenter__(self):
        self.ws = await websockets.connect(self.url, open_timeout=5)
        return self

    async def __aexit__(self, *exc):
        await self.ws.close()

    async def recv(self) -> dict:
        return json.loads(await self.ws.recv())

    async def send(self, block: str, cmd: str, args: dict | None = None) -> Any:
        """Send one command; return result or raise RuntimeError(error)."""
        msg_id = self.next_id
        self.next_id += 1
        await self.ws.send(json.dumps({"id": msg_id, "block": block, "cmd": cmd, "args": args or {}}))
        # Broadcasts (status/hello/alarm) interleave with the reply; skip them.
        while True:
            msg = await asyncio.wait_for(self.recv(), timeout=5)
            if msg.get("id") == msg_id:
                if msg.get("ok"):
                    return msg.get("result", {})
                raise RuntimeError(msg.get("error", "error"))

    async def next_status(self) -> dict:
        while True:
            msg = await asyncio.wait_for(self.recv(), timeout=5)
            if msg.get("type") == "status":
                return msg


def run(coro):
    try:
        return asyncio.run(coro)
    except (TimeoutError, OSError, websockets.WebSocketException) as e:
        typer.echo(f"connection failed: {e}", err=True)
        raise typer.Exit(1) from None
    except RuntimeError as e:
        typer.echo(f"error: {e}", err=True)
        raise typer.Exit(1) from None


HostOpt = typer.Option(
    None,
    "--host",
    "-H",
    help="board hostname or IP, e.g. instrument-639C.local on a shared network or 192.168.4.1 "
    "on the board's own access point (default: instrument.local, then 192.168.4.1)",
)


# --------------------------------------------------------------------------
# Commands
# --------------------------------------------------------------------------
@app.command()
def status(host: str | None = HostOpt, block: str | None = typer.Argument(None, help="only this block")):
    """Print one status broadcast as JSON."""

    async def go():
        async with Client(discover(host)) as c:
            msg = await c.next_status()
            data = msg["blocks"].get(block, {}) if block else msg["blocks"]
            typer.echo(json.dumps(data, indent=2))

    run(go())


@app.command()
def send(block: str, cmd: str, args: str = typer.Option("{}", help="JSON object"), host: str | None = HostOpt):
    """Send one command and print the result."""
    try:
        parsed = json.loads(args)
    except json.JSONDecodeError as e:
        typer.echo(f"--args is not valid JSON: {e}", err=True)
        raise typer.Exit(2) from None

    async def go():
        async with Client(discover(host)) as c:
            typer.echo(json.dumps(await c.send(block, cmd, parsed), indent=2))

    run(go())


@app.command()
def stream(
    key: str = typer.Argument(..., help="block.key, e.g. base.counter"),
    seconds: float = typer.Option(5.0, help="how long to record"),
    csv_path: str | None = typer.Option(None, "--csv", help="write t_ms,value rows here"),
    host: str | None = HostOpt,
):
    """Print (and optionally save) one status value at 20 Hz."""
    if "." not in key:
        typer.echo("key must be block.key (e.g. base.counter)", err=True)
        raise typer.Exit(2)
    blk, k = key.split(".", 1)

    async def go():
        rows = []
        async with Client(discover(host)) as c:
            t_end = time.monotonic() + seconds
            while time.monotonic() < t_end:
                msg = await c.next_status()
                v = msg["blocks"].get(blk, {}).get(k)
                rows.append((msg["t"], v))
                typer.echo(f"{msg['t']}\t{v}")
        if csv_path:
            with open(csv_path, "w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["t_ms", key])
                w.writerows(rows)
            typer.echo(f"wrote {len(rows)} rows to {csv_path}", err=True)

    run(go())


@alarms_app.command("list")
def alarms_list(host: str | None = HostOpt):
    """List alarm rules."""

    async def go():
        async with Client(discover(host)) as c:
            r = await c.send("alarms", "list")
            for x in r["rules"]:
                flag = "*" if x["active"] else " "
                rule = f"{x['block']}.{x['key']} {x['op']} {x['threshold']}"
                typer.echo(f"{flag} #{x['id']:<3} {rule}  ->  {x['action']}  (fired {x['fired']})")
            if not r["rules"]:
                typer.echo("no rules")

    run(go())


@alarms_app.command("add")
def alarms_add(
    block: str = typer.Option(...),
    key: str = typer.Option(...),
    op: str = typer.Option("gt", help="gt lt ge le eq ne"),
    threshold: float = typer.Option(...),
    action: str = typer.Option("notify", help="notify | relay:<n>:on | relay:<n>:off"),
    host: str | None = HostOpt,
):
    """Add an alarm rule."""

    async def go():
        async with Client(discover(host)) as c:
            r = await c.send("alarms", "add", {"block": block, "key": key, "op": op, "threshold": threshold, "action": action})
            typer.echo(f"added rule #{r['id']}")

    run(go())


@alarms_app.command("remove")
def alarms_remove(rule_id: int = typer.Argument(..., help="rule id from `alarms list`"), host: str | None = HostOpt):
    """Remove an alarm rule by id."""

    async def go():
        async with Client(discover(host)) as c:
            await c.send("alarms", "remove", {"id": rule_id})
            typer.echo(f"removed rule #{rule_id}")

    run(go())


def main():
    app()


if __name__ == "__main__":
    sys.exit(main())
