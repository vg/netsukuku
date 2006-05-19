<?php

	define('NTK_DOCROOT', 'http://netsukuku.freaknet.org/2html/documentation/');

	if (empty($_GET['dir'])) {
		$_GET['dir'] = "";
	} else {
		$_GET['dir'] .= "/";
	}

	if (!empty($_GET['file'])) {
		include(NTK_DOCROOT.$_GET['file']);
		exit(0);
	}

	$tpl_page = '<pre id="filez">';

function listing($arg)
{
	if ($arg > 1)
		exit(1);

	$foo = fopen(NTK_DOCROOT.$_GET['dir'].'.list', "r");

	while (!feof($foo)) {
		$line = fgets($foo);
		$line = htmlentities($line);
		if (ereg("/", $line)) { //e` directory
			$line = ereg_replace("/\n", '', $line);
			$tpl_page .= '- <a style="text-transform: uppercase; text-decoration: none;" href="documentazione2.php?dir='.$_GET['dir'].$line.'">'.strtoupper($line).'</a> - <br />';
			
			$file_info = @fopen(NTK_DOCROOT.$_GET['dir'].$line.".info", "r");
				if ($file_info != NULL) {
					//$file_info = fopen(NTK_DOCROOT.$_GET['dir'].$line.".info", "r");
                                        $info_line = fgets($file_info);
					$tpl_page .= $info_line . "<br />";
                                        $file_info = fclose($file_info);
				
					/*$file_listing = fopen(NTK_DOCROOT.$_GET['dir'].$line."/.list", "r");
					while (!feof($file_listing)) {
						$lista = fgets($file_listing);
						$tpl_page .= $lista;
					}
					$file_listing = fclose($file_listing);
					*/
					listing($arg++);


				} else {
					$tpl_page .= "<br />";
				}
		} else if (empty($line)) //e` empty line
			continue;
		else { //e` un file
			$line = ereg_replace("\n", '', $line);
			if (ereg(".info", $line))
				continue;
			else {
				$tpl_page .= '<a href="documentazione2.php?file='.$_GET['dir'].$line.'">'.$line.'</a>';
				$file_info = @fopen(NTK_DOCROOT.$_GET['dir'].$line.".info", "r");
					
				if ($file_info != NULL) {
					$info_line = fgets($file_info);
					$tpl_page .= ' --> ' . $info_line;
					$file_info = fclose($file_info);
				} else {
					$tpl_page .= "<br />";
				}
			}
				
		}
	}
			
	$foo = fclose($foo);

	$tpl_page .= "</pre>";
	print $tpl_page;
}
listing(0);
	
?>
