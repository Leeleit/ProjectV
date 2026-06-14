# ProjectV KT documents — Makefile
# Doom emacs: M-x compile → make

LATEXMK = latexmk -pdfxe -interaction=nonstopmode -halt-on-error -file-line-error
BUILDDIR = .tmp

DOCS := KT-2.1_Architecture KT-2.2_Test_Report KT-3.1_User_Guide KT-3.2_Final_Report
PDFS := $(addsuffix .pdf,$(DOCS))

.PHONY: all clean regen

all: $(PDFS) KT-Combined.pdf

# --- Pattern rule: each PDF built in its own subdir under .tmp/ ---

%.pdf: %.tex header.tex
	@mkdir -p $(BUILDDIR)/$*
	$(LATEXMK) -outdir=$(BUILDDIR)/$* $<
	@cp $(BUILDDIR)/$*/$@ .

# --- Per-document metadata deps (so changes trigger rebuild) ---

KT-2.1_Architecture.pdf: KT-2.1.yaml
KT-2.2_Test_Report.pdf: KT-2.2.yaml
KT-3.1_User_Guide.pdf: KT-3.1.yaml
KT-3.2_Final_Report.pdf: KT-3.2.yaml

# --- Combined: include all 4 fragments ---

KT-Combined.pdf: KT-Combined.tex $(addsuffix -frag.tex,$(DOCS)) header.tex
	@mkdir -p $(BUILDDIR)/KT-Combined
	$(LATEXMK) -outdir=$(BUILDDIR)/KT-Combined KT-Combined.tex
	@cp $(BUILDDIR)/KT-Combined/$@ .

clean:
	rm -rf $(BUILDDIR)
	rm -f $(PDFS) KT-Combined.pdf
	rm -f *.aux *.log *.fls *.fdb_latexmk *.toc *.out *.bbl *.blg *.synctex.gz
