<html>
<head>
<title>Netsukuku</title>
</head>
<link rel="stylesheet" type="text/css" href="style.css" />

<body bgcolor="#000000" topmargin="1" leftmargin="1">
	<table width="100%" cellspacing="0" cellpadding="0">
		<tr>
			<td><img src="logo_netsukuku.jpg" /></td>
		</tr>
		<tr>
			<table cellpadding="0" border="0" cellspacing="0" class="puppu">
				<tr>
			<td>&nbsp;&nbsp;&nbsp;</td>
			<td><a href="index.php?act=1" class="
				<?php 
					switch($_GET['act']){
						case 1:
							print "iam";
							break;
						case 2:
						case 3:
						case 4:
						default:
							print "menu";
							break;
					}
				?>
			"> Home</a></td>
			<td>&nbsp;&nbsp;&nbsp;</td>
			<td><a href="index.php?act=2" class="
				<?php 
                                        switch($_GET['act']){
                                                case 2:
                                                        print "iam";
                                                        break;
                                                case 1:
                                                case 3:
                                                case 4:
						default:
                                                        print "menu";
                                                        break;
                                        }
                                ?>		
			"> About </a></td>
			<td>&nbsp;&nbsp;&nbsp;</td>
			<td><a href="index.php?act=3" class="
				<?php 
                                        switch($_GET['act']){
                                                case 3:
                                                        print "iam";
                                                        break;
                                                case 2:
                                                case 1:
                                                case 4:
						default:
                                                        print "menu";
                                                        break;
                                        }
                                ?>
			"> Documentation </a></td>
			<td>&nbsp;&nbsp;&nbsp;</td>
			<td><a href="index.php?act=4" class="
				<?php 
                                        switch($_GET['act']){
                                                case 4:
                                                        print "iam";
                                                        break;
                                                case 2:
                                                case 3:
                                                case 1:
						default:
                                                        print "menu";
                                                        break;
                                        }
                                ?>
			"> Download </a></td>
				</tr>
			</table>
		</tr>
		<tr>
			<td>
					<pre>
		<?php
			switch($_GET['act']){
				case 1:
					include("home");
					break;
				case 2:
					include("http://www.hinezumilabs.org/viewcvs/*checkout*/netsukuku/README?rev=HEAD&content-type=text/plain");
					break;
				case 3:
					include("http://www.hinezumilabs.org/viewcvs/*checkout*/netsukuku/netsukuku?rev=HEAD&content-type=text/plain");
					break;
				case 4:
					include("http://netsukuku.freaknet.org/files");
					break;
				
				default :
					include("home");
					break;
			}
		?>
					</pre>
			</td>
		</tr>
	</table>
	<font color="9A9A9A">
	<pre>
	--
	design by <a href="mailto:crash@freaknet.org">crash</a> && <a href="mailto:black@bourneagain.jp">black</a>
	</pre>
	</font>
</body>
</html>
