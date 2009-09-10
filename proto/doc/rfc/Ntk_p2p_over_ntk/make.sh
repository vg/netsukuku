rm -v *.{pdf,dvi}

latex  --interaction nonstopmode *.tex &> /dev/null
latex  --interaction nonstopmode *.tex &> /dev/null && \
echo DVI compiled

pdflatex --interaction nonstopmode *.tex &> /dev/null
pdflatex --interaction nonstopmode *.tex &> /dev/null &&\
pdflatex *.tex &> /dev/null &&\
echo PDF compiled

#latex2html -split 0 -dir html -mkdir *.tex &> /dev/null &&\
#echo HTML compiled

cp Ntk_p2p_over_ntk.pdf ../../../../doc/main_doc/ntk_rfc/Ntk_p2p_over_ntk.pdf
