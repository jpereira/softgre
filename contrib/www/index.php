<?php
/**
 * Author: Jorge Pereira <jpereiran@gmail.com>
 * Date: Thu Oct  2 00:52:06 BRT 2014
 */

error_reporting(E_ALL);
ini_set('display_errors', 1);

require_once 'include/PHPTail.php';

$tail = new PHPTail("/var/log/softgred.log");

/**
 * We're getting an AJAX call
 */
switch(@$_GET['action'])
{
    case "log";
        if(isset($_GET['ajax']))  {
        	echo $tail->getNewLines($_GET['lastsize'], $_GET['grep'], $_GET['invert']);
	        die();
        }
        break;
    default:
}

?>

<html>
<head>
<link type="text/css" href="http://ajax.googleapis.com/ajax/libs/jqueryui/1.8.9/themes/flick/jquery-ui.css" rel="stylesheet"></link>
<style type="text/css">
    .results {
        padding-bottom: 20px;
    }
</style>

<script type="text/javascript" src="http://ajax.googleapis.com/ajax/libs/jquery/1.4.4/jquery.min.js"></script>
<script type="text/javascript" src="https://ajax.googleapis.com/ajax/libs/jqueryui/1.8.9/jquery-ui.min.js"></script>

<script type="text/javascript">
    /* <![CDATA[ */
    //Last know size of the file
    lastSize = 0; 
    //lastSize = <?php echo filesize($tail->log); ?>;
    //Grep keyword
    grep = "";
    //Should the Grep be inverted?
    invert = 0;
    //Last known document height
    documentHeight = 0; 
    $(document).ready(function(){

        //Set up an interval for updating the log. Change updateTime in the PHPTail contstructor to change this
        setInterval ( "updateLog()", <?php echo $tail->updateTime; ?> );
    });

    //This function queries the server for updates.
    function updateLog() {
        $.getJSON('<?php echo $_SERVER['SCRIPT_NAME']; ?>?action=log&ajax=1&lastsize='+lastSize + '&grep='+grep + '&invert='+invert, function(data) {
            lastSize = data.size;
            $.each(data.data, function(key, value) { 
                $("#results").append(value + '\n');
            });

            $("#results").animate({
                scrollTop:$("#results")[0].scrollHeight - $("#one").height()
                },1000,function(){
            })
        });
    }
    /* ]]> */
</script>

<title> SoftGREd </title>
</head>
<body bg="#FFFFFF">

<center>
    <h1>SoftGREd Status</h1>
</center>

Bridges
<hr>
<pre>
# brctl show
<?php
echo shell_exec("/sbin/brctl show");
?>
#

Log do SoftGREd
<hr>
<textarea cols='140' rows='20' id="results"></textarea>
<hr>
</pre>
</body>
</html>

