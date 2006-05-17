<?php

	define('NTK_DOCROOT', 'http://netsukuku.freaknet.org/2html/documentation/');

	//$dirs = array();
	//$files = array();
	if (empty($_GET['dir'])) {
		$_GET['dir'] = "";
	} else {
		$_GET['dir'] .= "/";
	}

	$tpl_page = '<pre id="filez">';

	$foo = fopen(NTK_DOCROOT.$_GET['dir'].'.list', "r");

	while (!feof($foo)) {
		$line = fgets($foo);
		if (ereg("/", $line)) { //e` directory
			$line = ereg_replace("/\n", '', $line);
			$tpl_page .= '<a href="documentazione2.php?dir='.$line.'">'.$line.'</a>';
			
			//print(NTK_DOCROOT.$_GET['dir'].$line.".info<br />");
		
			$file_info = @fopen(NTK_DOCROOT.$_GET['dir'].$line.".info", "r");
				if ($file_info != NULL) {
					//$file_info = fopen(NTK_DOCROOT.$_GET['dir'].$line.".info", "r");
                                        $info_line = fgets($file_info);
                                        $tpl_page .= ' --> ' . $info_line;
                                        $file_info = fclose($file_info);
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
				$tpl_page .= '<a href="documentazione2.php?file='.$line.'">'.$line.'</a>';
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
	
?>
