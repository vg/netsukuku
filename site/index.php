<?php
	//if ($_GET['pag'] == "download")
	//	header("Location: http://netsukuku.freaknet.org/?p=Download");
	if ($_GET['pag'] == "wiki")
		header("Location: http://lab.dyne.org/Netsukuku");
	if ($_GET['pag'] == "forum")
		header("Location: http://nariki.bourneagain.jp/ntk/index.php?pag=forum");
	if ($_GET['pag'] == "maps")
		header("Location: http://nariki.bourneagain.jp/ntk/index.php?pag=maps");

	/* forum compatibility */
	require_once("inc_var.php");
	
	if (($_GET['pag'] == "forum") && ($_GET['act1'] == "login")) {
		include_once("./login.php");
	}

	if (($_GET['pag'] == "forum") && ($_GET['act'] == "search") && ($_GET['action'] == "show_user") && (!empty($_GET['user_id']))) {
		include_once("./search.php");
	}

	if (($_GET['pag'] == "forum") && ($_GET['act'] == "search") && (($_GET['action'] == "show_unanswered")) || ($_GET['action'] == "show_subscriptions")) {
		include_once("./search.php");
	}

	if (($_GET['pag'] == "forum") && ($_GET['act'] == "search") && ($_GET['action'] == "search") &&(!empty($_GET['keywords'])) && (!empty($_GET['author'])) && (!empty($_GET['forum'])) && (!empty($_GET['search_in'])) && (isset($_GET['sort_by'])) && (!empty($_GET['sort_dir'])) && (!empty($_GET['show_as'])) && (!empty($_GET['search']))) {
		include_once("./search.php");
	}

	if (($_GET['pag'] == "forum") && ($_GET['act'] == "viewtopic") && ($_GET['action'] == "new") && (isset($_GET['id']))) {
		include_once("./viewtopic.php");
	}

	/* serve per effettuare controlli alternativi all'entropia dell'univers... AIUTO SONO PAZZO */
	if (!empty($_COOKIE['biscottino_patatina']) && empty($_SESSION['ntk_dio_info'])) {

                include('config.php');

                $cookie_val = unserialize($_COOKIE['biscottino_patatina']);

                $dio[0] = $cookie_val[0];

                $dbi = mysql_connect($db_host, $db_username, $db_password);
                mysql_select_db($db_name, $dbi);

                $fetch_info = mysql_query("SELECT username, password, save_pass FROM forum_users WHERE id = $cookie_val[0]");

                while ($buffer = mysql_fetch_array($fetch_info)) { // sistemare con mysql_fetch_field per la composizione dell'array
                        $dio[1] = $buffer['password'];
                        $mio[0] = $buffer['username'];
                        $mio[1] = $buffer['save_pass'];
                }
        
                mysql_close($dbi);
        
                unset($buffer);
        
                $dio[1] = md5($cookie_seed.$dio[1]);

                #print_r(array_values($dio)); // DA LEVARE
                #print("<br />");
                #print_r(array_values(unserialize($_COOKIE['biscottino_patatina'])));

                session_start();

                $_SESSION['ntk_dio_info'] = serialize($dio);
                $_SESSION['ntk_mio_info'] = serialize($mio);
        
                $username = $mio[0];
        
                if ($mio[1] > 0) /* tramite la funzione defined() posso verificare che l'utente voglia connettersi automaticamente */
                        define('AUTO_LOGIN', 1);
                
                unset($mio);
                unset($dio);

        }
	/* questo dipende dal controllo qui sopra */
	if (($_SESSION['ntk_dio_info'] == $_COOKIE['biscottino_patatina']) && !empty($_SESSION['ntk_mio_info']) && ($_GET['act'] == "register")) {
		header("Location: index.php?pag=forum");
	}

	if (($_GET['pag'] == "nodes") && (empty($_COOKIE['biscottino_patatina']) || ($username == "Guest")))
		header("Location: index.php?pag=home");

	/* END forum compatibility */
	
	if (($username == "Guest") || empty($_COOKIE['biscottino_patatina']))
		$pages = array('home', 'about', 'documentation', 'faq', 'download', 'contacts', 'wiki', 'forum', 'maps');
	else
		$pages = array('home', 'about', 'documentation', 'faq', 'download', 'contacts', 'wiki', 'forum', 'maps', 'nodes');
	
	$i = 0;
	
	if (empty($_GET['pag']))
		$_GET['pag'] = "home";

	/* aFARE IL CONTROLLO con diff
	foreach ($pages as $page)
		if ($_GET['pag']*/
	

	$menu .= "<ul>";
	foreach ($pages as $page) {
		if ($page == "forum")
			$menu .= "<li class=\"sep\">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</li>";
		$menu .= ($page == $_GET['pag']) ? "<li class=\"active\">&nbsp; " . $page . "&nbsp; </li>" : "<a href=\"?pag=$page\"><li class=\"men\">&nbsp; " . $page . "&nbsp; </li></a>" ;
		//$menu .= ($i < (count($pages) - 1)) ? " | " : "";
		$i++;
	}
	$menu .= "</ul>";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
	"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="it" lang="it">
<head>
<?php 
	if ($_GET['pag'] == "maps") {
	print("<script src=\"http://maps.google.com/maps?file=api&v=1&key=ABQIAAAA9mFsHT2mZQLK_L11ArNZUhQtBuTZdIQemk4Bq3Gapbo7u9TUTRTl9PIGBTh4hwZW31xlSYIa-J44rw\" type=\"text/javascript\">
	</script>");
	}
?>	
	<meta http-equiv="content-type" content="text/html; charset=UTF-8" />
	<meta name="generator" content="vim" />
	<meta name="author" content="black" />
	<meta name="keywords" content="netsukuku netsukuku.org" />
	<meta name="DC.title" content="netsukuku.org" />
	<title>Netsukuku Community</title>
	<script type="text/javascript"></script>
	<style type="text/css" media="all">
		@import "css/style.css";
	</style>
<?php
	if ($_GET['pag'] == "maps") {
        $dbi = mysql_connect('localhost', 'root', 'loredana') or die(mysql_error());
        mysql_select_db('netsukuku', $dbi) or die(mysql_error());

        $query = mysql_query("SELECT forum_users.username, ntk_nodes.lati, ntk_nodes.longi, ntk_nodes.nome, ntk_nodes.up, ntk_nodes.igw, ntk_nodes.copertura FROM ntk_nodes JOIN forum_users ON ntk_nodes.id_user = forum_users.id") or die(mysql_error());

        print("<script type=\"text/javascript\">
                //<![CDATA[

                function newMarker(point, username, lati, longi, nome, up, copertura) 
                {
                        var marker = new GMarker(point);

                        //IL FORM E` QUI
                        var html = \"<div style='font-size: 12px; font-weight: bold;'>\" + nome + \"</div><div style='font-size: 10px;'>User:&nbsp;\" + username + \"<br />Latitude:&nbsp;\" + lati + \"<br />Longitude:&nbsp;\" + longi + \"<br />Up:&nbsp;\" + up + \"<br /></div>\";
                        GEvent.addListener(marker, 'click', function() { marker.openInfoWindowHtml(html) } );
        
                        return marker;
                }

                function onLoad()
                {
                        //if (GBrowserIsCompatible()) {
                                var map = new GMap(document.getElementById(\"map\"));
                                map.setMapType (G_HYBRID_TYPE);
                                map.addControl(new GLargeMapControl());
                                map.addControl(new GMapTypeControl());
                                map.centerAndZoom(new GPoint(15.1326, 37.5431), 16);");
                        
        while ($row = mysql_fetch_array($query)) {
                $row['up'] = ($row['up'] > 0) ? "Yes" : "No";
                $row['copertura'] = (is_null($row['copertura'])) ? '-' : $row['copertura'] . " m";
                        
                         print("var point = new GPoint($row[longi], $row[lati]);
                                var marker = newMarker(point, \"$row[username]\", \"$row[lati]\", \"$row[longi]\", \"$row[nome]\", \"$row[up]\", \"$row[copertura]\");
                                map.addOverlay(marker);

                        ");
                                
        }               
                        //END FUNCTION onLoad()        
                        print("//}
                }       
                
                //]     
        
        </script>");

        mysql_close($dbi) or die(mysql_error());
	}
?>
</head>

<body onload="onLoad()">

	<div id="container"> <!-- CONTAINER START-->

		<div id="logo">
			<div style="float: left;"><img src="http://netsukuku.freaknet.org/logo_netsukuku.jpg" alt="" /></div>
			<?php
				if (($username == "Guest") || (empty($_COOKIE['biscottino_patatina']))) {
					print('<div id="login" style="float: right; padding-right: 10px; padding-top: 10px; color: #CCCCCC; ">Already a user? <a href="index.php?pag=forum&act=login&ntk=lgn" style="font-weight: bold; font-size: 12px;">Sign in!</a><br />Not registered? <a href="index.php?pag=forum&amp;act=register&amp;ntk=rgstr" style="font-weight: bold; font-size: 12px">Sign up!</a><br /></div>');
				} else {
					print('<div id="login" style="float: right; padding-right: 10px; padding-top: 10px; color: #DDD;">Welcome <span id="login" style="font-weight: bold; font-size: 12px;">'.$username.'</span>,<a href="index.php?pag=forum&amp;act1=login&amp;action=out&id='.$cookie_val[0].'" style="font-weight: bold;"> Sign out!</a></div>');
				}
			?>
		</div>

		<div id="sidebar"><br />
		<br />
		<?php 
			print("<form method=\"get\" action=\"index.php\">
				<input type=\"hidden\" name=\"pag\" value=\"maps\">");
			print $select_provinces; 
			
			if ($_GET['prn'] == 'ee')
				print $states_select;
			
			print("</form>"); 
		
		?>
		<br />
		</div>
		
		<div id="menu">
			<?php print $menu; ?>	
		</div>

		<div id="content">
			<?php
				if ($_GET['pag'] == 'maps')
				print "<center><div id=\"map\" style=\"width: 500px; height: 400px;\"></div></center>";

				switch ($_GET['pag']) {
					case $_GET['pag']:
						if ($_GET['pag'] != "forum")
							include("./inc/" . $_GET['pag'] . ".php");
						else if ($_GET['pag'] == "forum") {
							if (empty($_GET['act']))
								include_once("./index2.php");
							else
								include_once("./" . $_GET['act'] . ".php");
						}
						break;
				}
			?>
			<br />
			<br />
		</div>
		
		<div id="foot">
			- netsukuku.org is part of Netsukuku project. Please refer to its licence for further informations -
		</div>

	</div> <!-- CONTAINER END-->

</body>
</html>
