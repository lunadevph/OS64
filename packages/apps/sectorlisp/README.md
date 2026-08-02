# SectorLISP for OS64

Bare-metal OS64 port of the portable C reference from Justine Tunney's
ISC-licensed [`sectorlisp`](https://github.com/jart/sectorlisp). It retains the
IBM 7090-style compact object memory and McCarthy metacircular evaluator.
Bestline, locale, wide stdio, and process exit calls were replaced with the
OS64 application ABI. The OS64 port resets its bounded object arena between
top-level expressions instead of compacting a persistent heap. Enter `EXIT` or
press Ctrl+C to leave the REPL.
