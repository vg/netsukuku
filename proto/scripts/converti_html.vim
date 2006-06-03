syn on
set fdm=diff
"%foldopen!
run! syntax/2html.vim 
%s/^<pre>/<pre id="filez">/
1
delete 7
%
delete
delete
wq
q
