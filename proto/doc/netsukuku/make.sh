rm -v netsukuku.{pdf,dvi}

latex  --interaction nonstopmode netsukuku.tex &> /dev/null
latex  --interaction nonstopmode netsukuku.tex &> /dev/null && \
echo DVI compiled

pdflatex --interaction nonstopmode netsukuku.tex &> /dev/null
pdflatex --interaction nonstopmode netsukuku.tex &> /dev/null &&\
pdflatex netsukuku.tex &> /dev/null &&\
echo PDF compiled

#latex2html -split 0 -dir html -mkdir netsukuku.tex &> /dev/null &&\
#echo HTML compiled
