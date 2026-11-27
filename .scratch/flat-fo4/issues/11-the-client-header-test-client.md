# The client the client header is tried with

Type: grilling
Status: resolved

## Question

What client proves that the client header finds the flat build (milestone 3)?

- the framework's mod template uses `vrcf` and `vrui` (`mod-template/src/MyMod.cpp`, `Config.cpp`), so building it for flat means guarding it
- or a small test client of its own

## Answer

Decided with the user, 2026-11-27.

A small test client in this repository: a minimal F4SE plugin built for flat that includes the client header and registers one page, built only for testing. The mod template stays VR-only.
