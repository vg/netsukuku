<?php

	define('NTK_DOCROOT', 'http://netsukuku.freaknet.org/2html/documentation/');

	//$dirs = array();
	//$files = array();

	$tpl_page = '<pre id="filez">';

	$foo = fopen(NTK_DOCROOT.'.list', "r");

	while (!feof($foo)) {
		$line = fgets($foo);
		if (ereg("/", $line)) //e` directory
			$tpl_page .= '<a href="index.php?pag=documentation&amp;dir='.$line.'">'.$line.'</a>';
		else if (empty($line)) //e` empty line
			continue;
		else { //e` un file
			/*if (ereg(".info", $line)) { //e` un file .info, va messo subito dopo il file caricato precedentemente
				$line = ereg_replace(' ', '', $line);
				$file_info = fopen(NTK_DOCROOT.$line, "r");
				//$info_line = fgets($file_info);
				//$tpl_page .= ' --> '. $info_line;
				$file_info = fclose($file_info);
			}*/
			//$file_info = fopen(NTK_DOCROOT . 'articles.info ', "r");
			$file_info = fopen("http://netsukuku.freaknet.org/2html/documentation/".$line, "r");
			$info_line = fgets($file_info);
			$tpl_page .= ' --> ' . $info_line;
			$file_info = fclose($file_info);
					
		}
			
	}

	$foo = fclose($foo);

	$tpl_page .= "</pre>";
	print $tpl_page;
	
?>
