rm -v qspn.{pdf,dvi}

pdflatex --interaction nonstopmode qspn.tex &> /dev/null
pdflatex --interaction nonstopmode qspn.tex &> /dev/null &&\
echo PDF compiled

latex  --interaction nonstopmode qspn.tex &> /dev/null
latex  --interaction nonstopmode qspn.tex &> /dev/null && \
echo DVI compiled

#latex2html -split 0 -dir html -mkdir qspn.tex &> /dev/null &&\
#echo HTML compiled
