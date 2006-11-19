rm -v andna.{pdf,dvi}

latex  --interaction nonstopmode andna.tex &> /dev/null
latex  --interaction nonstopmode andna.tex &> /dev/null && \
echo DVI compiled

pdflatex --interaction nonstopmode andna.tex &> /dev/null
pdflatex --interaction nonstopmode andna.tex &> /dev/null &&\
pdflatex andna.tex &> /dev/null &&\
echo PDF compiled

#latex2html -split 0 -dir html -mkdir andna.tex &> /dev/null &&\
#echo HTML compiled
