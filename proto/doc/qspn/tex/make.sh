rm -v qspn.{pdf,dvi}

pdflatex --interaction nonstopmode qspn.tex &> /dev/null
pdflatex --interaction nonstopmode qspn.tex &> /dev/null &&\
echo PDF compiled

latex  --interaction nonstopmode qspn.tex &> /dev/null
latex  --interaction nonstopmode qspn.tex &> /dev/null && \
echo DVI compiled
