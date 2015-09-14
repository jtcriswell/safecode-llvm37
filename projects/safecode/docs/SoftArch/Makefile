ifndef TARGET
TARGET=softarch
endif

DIA=/home/vadve/ensink/local/x86/bin/dia
DOT=/home/vadve/lattner/local/fc1/bin/dot

LATEX=latex
TEXFILES = ${wildcard *.tex} ${wildcard Tables/*.tbl}
BIBFILES = ${wildcard *.bib} ${wildcard $(HOME)/Papers/bibtex/*.bib}

DIAFILES = $(wildcard figs/*.dia)
FIGFILES = $(DIAFILES:%.dia=%.eps)

DOTFILES = ${wildcard figs/*.dot}
FIGFILES +=${DOTFILES:%.dot=%.ps}

figs/%.eps: figs/%.dia
	$(DIA) --export=$@ --nosplash $<

figs/%.ps: figs/%.dot
	$(DOT) -Tps < $< > $@

.PRECIOUS: %.ps %.pdf

all: $(TARGET).pdf

$(TARGET).dvi: $(TEXFILES) $(FIGFILES) $(BIBFILES) $(wildcard figs/*.tbl)  $(wildcard Tables/*.ps) $(DOTFILES)
	-rm -f *.aux
	$(LATEX) $(TARGET).tex
	#bibtex $(TARGET)
	$(LATEX) $(TARGET).tex
	$(LATEX) $(TARGET).tex
	@/bin/echo ""
	@/bin/echo "+++++++++++++++++++ Undefined References +++++++++++++++++"
	-@grep -i undef $(TARGET).log
	@/bin/echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	@/bin/echo ""

clean:
	rm -f *.aux *.bbl *.dvi *.lof *.log *.toc *.lot *.blg

%.ps: %.dvi
	dvips -o $@ $<  -t letter

%.pdf: %.ps
	ps2pdf $<
