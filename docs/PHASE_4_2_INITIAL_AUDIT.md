# Phase 4.2 Initial Audit: PDF Parser Replacement & Extraction Compliance

**Project:** Universal Smart Search Engine — Release 1  
**Phase:** 4.2 — PDF Parser Replacement & Extraction Compliance  
**Author:** Sibtain Abbas (`sibtaintahir512@gmail.com`)  
**Date:** November 8, 2026  
**Status:** AUDIT COMPLETE — IMPLEMENTATION IN PROGRESS  

---

## 1. Overview & Objectives

Phase 4.1 made significant improvements by integrating `miniz` and `pugixml` and introducing `PDFDocumentParser`. However, several critical architectural and security gaps remain to achieve full compliance:

1. **PDF Parsing Limitations:** The existing `PDFDocumentParser` relies on string search heuristics (`stream`, `endstream`, `BT`, `ET`) which can misidentify binary data inside images or fonts, cannot handle complex Type3/CID font encodings, and lacks full layout and reading-order awareness.
2. **XML Parsing Unsafe Fallback:** In `OfficeExtractor::stripXmlAndDecode`, if `pugixml` fails to parse a malformed XML document, it falls back to a raw `<...>` tag-stripping loop. This violates strict XML security principles.
3. **ZIP Decompression Security Arithmetic:** `miniz` ratio enforcement in `OfficeExtractor` used integer division `(uncomp / comp) > ratio`, which is susceptible to zero-division or truncation, and did not check `m_comp_size == 0` when `m_uncomp_size > 0`.
4. **XLSX Worksheet Ordering:** Worksheets are currently collected via `xl/worksheets/sheet*.xml` and sorted lexicographically, ignoring the explicit sheet order defined by `xl/workbook.xml` and `xl/_rels/workbook.xml.rels`.
5. **Streaming Output Bounds:** Extractor implementations must enforce the maximum extraction budget during document iteration rather than accumulating all text and only truncating after processing finishes.

---

## 2. Evaluation of PDF Parsing Libraries

| Library | License | Windows / MSVC | macOS / Clang | Dependencies | Unicode / CID | Verdict |
|:---|:---:|:---:|:---:|:---:|:---:|:---|
| **Poppler** | GPLv2 / GPLv3 | Complex (requires pkg-config, glib, cairo) | Available via Homebrew | Heavy UNIX stack | Excellent | **Rejected:** GPL license contagion and heavy runtime dependencies. |
| **MuPDF** | AGPLv3 / Commercial | Complex custom make/ninja | Available | Third-party fonts/libs | Good | **Rejected:** AGPL license incompatible with commercial desktop distribution. |
| **PDFium** | **Apache 2.0 / BSD** | **Official MSVC x64 prebuilt DLL/LIB** | **Official macOS dylib (Apple Silicon + x86)** | **Zero external dependencies** | **Full CMap, CID, embedded fonts, reading order** | **SELECTED:** Google's production engine used in Chromium; permissive license; complete API. |

---

## 3. Plan of Remediation

1. **Task A (PDF Engine):** Replace custom stream scanning with official Google PDFium (`fpdfview.h`, `fpdf_text.h`, `fpdf_doc.h`).
2. **Task B (XML Security):** Remove the unsafe fallback from `OfficeExtractor::stripXmlAndDecode`. Return `ExtractionStatus::Corrupted` if XML parsing fails.
3. **Task C (ZIP Security):** Use overflow-safe ratio check `uncomp > comp * maxRatio` and explicitly reject `comp == 0 && uncomp > 0`. Validate bounds before allocation.
4. **Task D (XLSX Order):** Parse `xl/workbook.xml` and `xl/_rels/workbook.xml.rels` to determine author-defined sheet order.
5. **Task E (Output Limits):** Bound text accumulation in each extractor loop.
6. **Task F & G (Tests & Regression):** Create realistic PDF and Office test fixtures; maintain 100% pass rate.
