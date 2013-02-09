latex_files=
include kernel/make.inc

all: pdf

pdf:
	@-mkdir -p built
	@for i in ${latex_files}; do \
		pdflatex -output-directory=built $$i ;\
	done

aux_clean:
	@-rm -rf built/*.aux built/*.dvi built/*.bak built/*.log 2>/dev/null

clean:
	@-rm -rf built 2>/dev/null
