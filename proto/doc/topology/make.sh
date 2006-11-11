rm -v topology.{pdf,dvi}

latex  --interaction nonstopmode topology.tex &> /dev/null
latex  --interaction nonstopmode topology.tex &> /dev/null && \
echo DVI compiled

pdflatex --interaction nonstopmode topology.tex &> /dev/null
pdflatex --interaction nonstopmode topology.tex &> /dev/null &&\
pdflatex topology.tex &> /dev/null &&\
echo PDF compiled

#latex2html -split 0 -dir html -mkdir topology.tex &> /dev/null &&\
#echo HTML compiled
