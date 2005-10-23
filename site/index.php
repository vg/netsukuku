<?php
//error_reporting(E_ERROR|E_WARNING|E_PARSE|E_NOTICE);

$pages = array(
    /* pagename	    => array(english, italian) */
    "Home"	    => array("home", ""),
    "About"	    => array("README", ""),
    "Documentation" => array("netsukuku",
			     "netsukuku.ita"),	
    "FAQ"	    => array("doc/FAQ", ""),
    "Download"	    => array("http://netsukuku.freaknet.org/files", ""),
    "Contacts"	    => array("contacts", ""),
);

/* default page = home */
if(isset($_GET['p']) && isset($pages[$_GET['p']]))
    $page = $_GET['p'];
else
    $page = "Home";

/* default language = english */
if(isset($_POST['lang']) && $_POST['lang'] == "it")
	$lang = "it";
else
	$lang = "en";


$navbar = "";
foreach($pages as $pname => $pdata) {
    $pclass = ($pname == $page) ? "iam" : "menu";
    $navbar .= "<td>&nbsp;&nbsp;&nbsp;</td>".
	"<td><a href=\"?p=$pname\" class=\"$pclass\">$pname</a></td>";
}

$it_selected = $en_selected = "";
if($lang == "it" && strlen($pages[$page][1])) {
    $to_include = $pages[$page][1];
    $it_selected = "selected=\"yes\"";
} else {
    $to_include = $pages[$page][0];
    $en_selected = "selected=\"yes\"";
}

$content = file_get_contents($to_include);
if(! ereg("^http:", $to_include)) {
	$content = htmlentities($content);
   	$content = preg_replace("/(http:\/\/[^\s\)]*)/", "<a href=\"\\1\">\\1</a>", $content);
   
}

?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<title>Netsukuku</title>
</head>
<link rel="stylesheet" type="text/css" href="style.css" />

<body bgcolor="#000000" topmargin="1" leftmargin="1" text="white">
	<table width="100%" cellspacing="0" cellpadding="0">
	<tr>
		<td><img src="logo_netsukuku.jpg" /></td>
	</tr>
	<tr>
		<td>
		<table cellpadding="0" border="0" cellspacing="0" class="puppu">
		<tr>
		<?php echo $navbar;?>
		</tr>
		</table>
		</td>
	</tr>
	<?php
	if($page == "Documentation") {
	?>
	<tr>
	<td>
		<br />
		<table>
		<tr><td class="manpage">
		<b>Man Pages</b><br />
		<a href="http://netsukuku.freaknet.org/doc/man_html/andna.html">andna</a><br />
		<a href="http://netsukuku.freaknet.org/doc/man_html/netsukuku_d.html">netsukuku_d</a><br />
		<a href="http://netsukuku.freaknet.org/doc/man_html/netsukuku_wifi.html">netsukuku_wifi</a><br />
		</td>
		<td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>
		<td>
		<form action="" method="post">
		<select name="lang" size="1" onchange="this.form.submit();">
		<option value="en" <?php echo $en_selected; ?>>English</option>
		<option value="it" <?php echo $it_selected; ?>>Italiano</option>
		</select>
		</form>
		</td>
		</tr>
		</table>
	
	</td>
	</tr>
	<?php } ?>

	<tr>
		<td>
		<pre>
		<?php echo $content; ?>
		</pre>
		</td>
		</tr>
	</table>
	<font color="#9A9A9A">
	<form action="https://www.paypal.com/cgi-bin/webscr" method="post">
	<input type="hidden" name="cmd" value="_s-xclick">
	<input type="image" src="https://www.paypal.com/en_US/i/btn/x-click-but21.gif" border="0" name="submit" alt="Make payments with PayPal - it's fast, free and secure!">
	<input type="hidden" name="encrypted" value="-----BEGIN PKCS7-----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-----END PKCS7-----
	">
	</form>
	<pre>
	--
	design by <a href="mailto:crash@freaknet.org">crash</a> && <a href="mailto:black@autistici.org">black</a> && <a href="mailto:e@entropika.net">entropika</a>
	</pre>
	</font>
</body>
</html>
