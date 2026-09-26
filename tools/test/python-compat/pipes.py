"""Compatibility shim for vendor runners using the removed pipes.quote."""

from shlex import quote

__all__ = ['quote']
