# Rhythm Layout Builder

Prototype: choose an official song slot and a difficulty, generate editor objects, and use **Improve** for a non-destructive refinement pass. If `<mod save directory>/song_N.wav` exists, the mod can scan its full PCM waveform (N=0..21). Otherwise a configurable approximate tempo preset is used; the generator does **not** analyze actual song audio in that case.

**Back up levels before testing!** The generator has no automatic playability verification. No song files are distributed.
