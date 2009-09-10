<?php

function getform($title, $mesg, $href, $link)
{
$form = <<<EOF
<div style="font-size: 12px; font-weight: bold;">$title</div><div style="font-size: 10px;">$mesg<br />Link: <a href="$href">$link</a></div>
EOF;

        return $form;
}       


?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="it" lang="it">
<head>
        <title>black Gmaps experiment</title>
        <script src="http://maps.google.com/maps?file=api&v=1&key=ABQIAAAA9mFsHT2mZQLK_L11ArNZUhQtBuTZdIQemk4Bq3Gapbo7u9TUTRTl9PIGBTh4hwZW31xlSYIa-J44rw" type="text/javascript">
        </script>
	
<?php
	$dbi = mysql_connect('localhost', 'root', 'loredana') or die(mysql_error());
	mysql_select_db('netsukuku', $dbi) or die(mysql_error());

	$query = mysql_query("SELECT forum_users.username, ntk_nodes.lati, ntk_nodes.longi, ntk_nodes.nome, ntk_nodes.up, ntk_nodes.igw, ntk_nodes.copertura FROM ntk_nodes JOIN forum_users ON ntk_nodes.id_user = forum_users.id") or die(mysql_error());

	print("<script type=\"text/javascript\">
		//<![CDATA[

                function newMarker(point, username, lati, longi, nome, up, copertura) 
                {
                        var marker = new GMarker(point);

                        //IL FORM E` QUI
			var html = \"<div style='font-size: 12px; font-weight: bold;'>\" + nome + \"</div><div style='font-size: 10px;'>User:&nbsp;\" + username + \"<br />Latitudine:&nbsp;\" + lati + \"<br />Longitudine:&nbsp;\" + longi + \"<br />Up:&nbsp;\" + up + \"<br />Copertura:&nbsp;\" + copertura + \"</div>\";
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
                                map.centerAndZoom(new GPoint(15.1326, 37.5431), 0);");
        
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
?>

</head>
<body onload="onLoad()">
        <!-- Predispongo una div 500x400 che contenga una mappa -->
        <div id="map" style="width: 500px; height: 400px">
        </div>
</body>
</html>


