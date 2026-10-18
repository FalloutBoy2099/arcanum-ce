# Buffered ART frame decoding

RLE frames previously read each command and repeated color through separate
`tig_file_fread` calls. They now read their encoded payload once, then decode
from memory with `tig_art_decode_rle`. One scratch buffer is reused throughout
the ART load and released before caching the decoded artwork. Raw frames retain
their existing bulk reads. No rendering, palette, animation or mirroring rules
are changed.

Frame table sizes, signed dimensions and aggregate pixel sizes are checked
before allocation. Compressed packets must consume the expected output exactly;
truncated/overflowing packets cannot write beyond the destination buffer.

## Validation

- 1,000 generated valid mixed literal/run streams match an independent old-style
  decoder, including zero-length packets and boundary canaries.
- Truncated packets, output overflow/underflow and invalid arguments are rejected.
- The actual loader is tested with mixed compressed/raw/empty synthetic frames,
  and rejects invalid dimensions, frame counts and malformed RLE payloads.
- Every frame/pixel of the option arrow, PageTurn_U and French font artwork matches
  the independent decoder.
- Manual read-only probes additionally compare every frame and all eight rotations
  of two installed character animations against that oracle.
- 22 CTest suites pass; no visual gameplay check is claimed.

## Offline probe

The probe alternates old/new reader order, using the same TIG file stream. Its
timings include encoded-pixel reads and decoding, exclude seeking/opening,
palette setup and rendering, and total ten repetitions of every compressed frame.
These are warm-file-cache microbenchmarks, not FPS measurements.

| Artwork | Old total (ms) | New total (ms) | TIG reads old/new |
| --- | ---: | ---: | ---: |
| HMM/hmmlaxag.ART | 20.7496 | 3.1645 | 606680 / 800 |
| HGM/hgmv1xag.ART | 31.8691 | 5.4304 | 927640 / 800 |

The probe is in `out/art-decode-game-probe.log` and runs via `art_decode_test.exe`
with absolute artwork paths as arguments. Test fixtures are created temporarily
and game files are only read.

The three journal tabs with approximately 170 ms load spikes contain raw frames;
RLE batching cannot explain or guarantee removal of those stalls. Additional
`art_file_open`, `art_metadata` and `art_pixels` profiler metrics identify which
part of the complete load stalls on the next gameplay capture. Metrics remain
inclusive, and slow operations retain their resource path in the existing sidecar.
