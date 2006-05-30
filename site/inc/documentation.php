<?php

	$languages_expr='/' . '\.en$' . '|' . '\.ita$' . '|' . '\.fr$' . '|' . '\.ru$' . '|' . '\.spa$' . '|';
	$languages_expr.= '\.chi$' . '|' . '\.nl$' . '|' . '\.jp$' . '/';

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

	$foo = fopen(NTK_DOCROOT.$_GET['dir'].'.list', "r");

	while (!feof($foo)) {
		$line = fgets($foo);
		if (ereg("/", $line)) { //e` directory
			$line = ereg_replace("/\n", '', $line);
			$tpl_page .= '<a href="index.php?pag=documentation&amp;dir='.$_GET['dir'].$line.'">';
			$tpl_page .= '[ ' . $line. ' ]' . '</a>';
			
			$file_info = @fopen(NTK_DOCROOT.$_GET['dir'].$line.".info", "r");
				if ($file_info != NULL) {
                                        $info_line = fgets($file_info);
                                        $tpl_page .= '<br>' . $info_line . '<br><br>';
                                        $file_info = fclose($file_info);
				} else {
					$tpl_page .= "<br />";
				}
		} else if (empty($line)) //e` empty line
			continue;
		else { //e` un file
			$line = ereg_replace("\n", '', $line);
			if (ereg(".info", $line)) {
				continue;
			} else if (preg_match($languages_expr, $line)) {
				//file.lang
				continue;
			} else {
				$tpl_page .= '<a href="index.php?pag=documentation&amp;file='.$_GET['dir'].$line.'">'.$line.'</a>';

				$file_info = @fopen(NTK_DOCROOT.$_GET['dir'].$line.".info", "r");
					
				if ($file_info != NULL) {
					$parse_cmd = 'cat '. '2html/documentation/' . $_GET['dir'];
					$parse_cmd.= '.list' . '| ./inc/parse_lang.sh ' . $line . ' ' . $_GET['dir'];
					$tpl_lang = system($parse_cmd);

					$info_line = fgets($file_info);
					$tpl_page .= ' ' . $tpl_lang . ' --> ' . $info_line;
					$file_info = fclose($file_info);
				} else {
					$tpl_page .= "<br />";
				}
			}
				
		}
	}
			
	$foo = fclose($foo);


	if ((ereg("/", $_GET['dir'])) || (ereg("/", $_GET['file']))) {
		$tpl_page .= "<br /><br /><a href=\"index.php?pag=documentation\"<-- Back</a>";
	}

	$tpl_page .= "</pre>";

	print $tpl_page;
	
?>
