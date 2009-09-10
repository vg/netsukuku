<?
if(isset($mod)){
 if(is_numeric($mod)){
 if($mod >= 0){

$log="counter.txt";
$write=fopen($log,'w');
fputs($write,$mod);
fclose($write);
include('counter.txt');

} else {
echo "thats not positive";
}
} else {
echo "thats not a number";
}
} else {
$log="counter.txt";
$open=@fopen($log,'r+');
$counter=@fread($open,filesize($log));
@fclose($open);
$counter++;
$write=fopen($log,'w');
fputs($write,$counter);
fclose($write);
#echo "$counter";
}

?>
