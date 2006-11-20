rm -v inetntk.{pdf,dvi}

latex  --interaction nonstopmode inetntk.tex &> /dev/null
latex  --interaction nonstopmode inetntk.tex &> /dev/null && \
echo DVI compiled

pdflatex --interaction nonstopmode inetntk.tex &> /dev/null
pdflatex --interaction nonstopmode inetntk.tex &> /dev/null &&\
pdflatex inetntk.tex &> /dev/null &&\
echo PDF compiled

#latex2html -split 0 -dir html -mkdir inetntk.tex &> /dev/null &&\
#echo HTML compiled
