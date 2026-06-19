# Makefile — defense presentation
# Использование:
#   make                — собрать DefensePresentation.pdf
#   make notes          — собрать с заметками спикера (для репетиции)
#   make clean          — удалить .aux/.log/.out/.toc/.nav/.snm/.vrb/.fls/.fdb_latexmk
#   make clean-all      — clean + удалить .pdf

MAIN = DefensePresentation
LATEXMK = latexmk
LATEXMK_FLAGS = -pdfxe -interaction=nonstopmode -halt-on-error -file-line-error

.PHONY: all notes clean clean-all

all: $(MAIN).pdf

$(MAIN).pdf: $(MAIN).tex header.tex screenshots/voxel_lab.png
	$(LATEXMK) $(LATEXMK_FLAGS) $(MAIN).tex

notes:
	$(LATEXMK) $(LATEXMK_FLAGS) -preamble='\usepackage{beamerarticle}\setjobnamebeamerversion{$(MAIN)Notes}' $(MAIN).tex

clean:
	rm -f *.aux *.log *.out *.toc *.nav *.snm *.vrb *.fls *.fdb_latexmk *.bbl *.blg *.idx *.ilg *.ind *.lof *.lot *.run.xml

clean-all: clean
	rm -f $(MAIN).pdf
