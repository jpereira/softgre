<?php
/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
 */
require_once("config.php");
require_once("lib/Rest.inc.php");
require_once("lib/Helper.inc.php");

class SoftGREd extends REST {
   /**
    * QUIT - desc: quit connection
    * LMIP - desc: list macs by IP of CPE, syntax: LMIP <ip of cpe>
    * GTMC - desc: get tunnel infos by MAC of client, result: $iface, $ip_remote, syntax: GTMC <mac of client>
    * STUN - desc: show list with all provisioning, result: $ip_remote1@$iface1;$ip_remoteN@$ifaceN;... 
    * STAT - desc: show status of SoftGREd
    */
    private $_cmd_map = array("ues"     => array("cmd" => "LMIP", "need_arg" => true),
                              "cpe"     => array("cmd" => "GTMC", "need_arg" => true),
                              "tunnels" => array("cmd" => "STUN", "need_arg" => false),
                              "status"  => array("cmd" => "STAT", "need_arg" => false),
                              "help"    => array("cmd" => "HELP", "need_arg" => false)

    );

    private $softgred_host = $cfg["DEFAULT_SOFTGRED_HOST"];
    private $softgred_port = $CFG["DEFAULT_SOFTGRED_PORT"];
    private $data = "";
    private $gred = NULL;

    /**
     *  SoftGREd Connection 
     */
    public function __construct($softgred_host=null, $softgred_port=null)
    {
        parent::__construct();

        if ($softgred_host)
            $this->softgred_host = $softgred_host;

        if ($softgred_port)
            $this->softgred_port = $softgred_port;

        $this->sock = @socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
        if(!$this->sock)
        {
            $result = array(
                'status' => 'error',
                'body' => "socket_create() failed: reason: " . socket_strerror(socket_last_error()) 
            );
            $this->response($this->json($result), 406);
        }
    }

    /**
     *  SoftGREd Disconnection 
     */
    public function __destruct()
    {
        if (@socket_close($this->sock) != 0)
        {
            $result = array(
                'status' => 'error',
                'body' => "socket_close() failed: reason: " . socket_strerror(socket_last_error()) 
            );
            $this->response($this->json($result), 406);
        }
    }
    
    /**
     * Public method for access api.
     * This method dynmically call the method based on the query string
     */
    public function run()
    {
        $method = $this->get_request_method();
        if($method != "POST" && $method != "GET")
        {
            $this->response("Only GET or POST!", 406);
        }

        $func = $this->get_func();
        $args = $this->get_args();

        if (!array_key_exists($func, $this->_cmd_map))
        {
            $this->response(sprintf("the function '%s' don't exist", $func?$func : "null"), 404);
        }

        $this->wrapper($func, $args);
    }

    private function wrapper($func, $args)
    {
        $arg = key($args);
        $map = $this->_cmd_map[$func];
        $cmd = $map['cmd'];
        $cmd_need_arg = $map['need_arg'];

        if ($cmd_need_arg)
        {
            if (empty($arg))
            {
                $error = array('status' => "Failed", "body" => "Command ".$func." needs argument");
                $this->response($this->json($error), 400);
            }
        }

        if (isset($cmd) && isset($arg))
        {
            $result = $this->wrapper_exec($cmd, $arg);
            $this->response($this->json($result), 200);
        }
        
        // If invalid inputs "Bad Request" status message and reason
        $error = array('status' => "Failed", "body" => "Invalid params, expected ue=mac-of-ue or cpe=ip-of-cpe");
        $this->response($this->json($error), 400);
    }

    private function wrapper_exec($cmd, $arg)
    {
        $result = @socket_connect($this->sock, $this->softgred_host, $this->softgred_port);
        if (!$result)
        {
            $result = array(
                'status' => 'error',
                'body' => sprintf("socket_connect(%d, %s, %s) reason of failure is: %s", 
                                $this->sock, $this->softgred_host, $this->softgred_port, socket_strerror(socket_last_error()))
            );
            $this->response($this->json($result), 406);
        }

        $buf = null;
        $out = array();
        $query = $cmd . " " . $arg;
        $query = strip_tags($query);
        socket_write($this->sock, $query, strlen($query));

        if (($buf = socket_read($this->sock, 4096)) < 0)
            return null;

        // TODO: ugly 'gohorse', just for fix a little problem in server side.
        $buf .= socket_read($this->sock, 4096);

        $buf = explode("\n", $buf);
        if ($cmd == "HELP")
        {
            // from
            foreach (array_values($this->_cmd_map) as $k => $v)
                $from[] = $v['cmd'];

            // to
            foreach (array_keys($this->_cmd_map) as $k => $v)
                $to[] = "GET /softgre/api/$v";

            $out['status'] = "OK";
            $out['body'] = str_replace($from, $to, $buf);
        }
        else
        {
            foreach ($buf as $line)
            {
                @list($key, $val) = @explode(": ", $line);

                if (!$line)
                    continue;

                if (!strcmp($key, "RESULT"))
                    $out['status'] = trim($val);
                
                if (!strcmp($key, "BODY"))
                    $out['body'] = strip_tags(trim($val));
            }
        }

        return $out;
    }    

    /**
     *    Encode array into JSON
     */
    private function json($data)
    {
        if (is_array($data))
        {
            return json_encode($data);
        }
    }
}

// Initiiate Library
$api = new SoftGREd($cfg["SOFTGRED_HOST"], $cfg["SOFTGRED_PORT"]);
$api->run();
?>

