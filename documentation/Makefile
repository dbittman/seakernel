latex_files=
latex_chapter_files=chapters/chapter_locks.tex \
					chapters/chapter_datastructures.tex
include kernel/make.inc

all: pdf

pdf:
	@-mkdir -p built/individual
	@for i in ${latex_files}; do \
		pdflatex -output-directory=built/individual $$i ;\
	done
	
	@-mkdir -p built/chapters
	@for i in ${latex_chapter_files}; do \
		pdflatex -output-directory=built/chapters $$i ;\
	done
	
	@pdflatex -output-directory=built main.tex
	
	@$(MAKE) -s aux_clean
	
aux_clean:
	@-find . -type f -name "*.aux" -exec rm -f {} \;
	@-find . -type f -name "*.log" -exec rm -f {} \;

clean:
	@-rm -rf built 2>/dev/null
