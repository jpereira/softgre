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
require_once("lib/Rest.inc.php");

class API extends REST {
    const SOFTGRED_SERVER = "localhost";
    const SOFTGRED_PORT   = "8888";

    private $data = "";
    private $gred = NULL;

    public function __construct()
    {
        parent::__construct();
        $this->softgredConnect();
    }

    public function __destruct()
    {
        if (socket_close($this->sock) != 0)
        {
            $result = array(
                'status' => 'error',
                'msg' => "socket_close() failed: reason: " . socket_strerror(socket_last_error()) 
            );
            $this->response($this->json($result), 406);
        }
    }

    /**
     *  SoftGREd Connection 
     */
    private function softgredConnect()
    {
        $this->sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
        if(!$this->sock)
        {
            $result = array(
                'status' => 'error',
                'msg' => "socket_create() failed: reason: " . socket_strerror(socket_last_error()) 
            );
            $this->response($this->json($result), 406);
        }
    }
    
    private function softgredCmd($cmd, $arg)
    {
        $result = socket_connect($this->sock, self::SOFTGRED_SERVER, self::SOFTGRED_PORT);
        $out = array();

        if (!$result)
            return null;
    
        if ($cmd)
        {
            socket_write($this->sock, $cmd, strlen($cmd));

            if (($buf = socket_read($this->sock, 4096)) < 0)
                return null;
            
            foreach (explode("\n", $buf) as $line)
            {
                @list($key, $val) = explode(": ", $line);

                if (!$line)
                    continue;

                if (!strcmp($key, "RESULT"))
                    $out['status'] = trim($val);
                
                if (!strcmp($key, "BODY"))
                    $out['body'] = trim($val);
            }
        }
        
        return $out;
    }

    /*
     * Public method for access api.
     * This method dynmically call the method based on the query string
     *
     */
    public function processApi()
    {
        @$func = strtolower(trim(str_replace("/","",$_REQUEST['rquest'])));
        if((int)method_exists($this, $func) > 0)
        {
            $this->$func();
        }
        else
        {
            $this->response('',404);
        }
    }
    
    private function lookup()
    {
        $me = $this->get_request_method();
        if($me != "POST" && $me != "GET")
        {
            $this->response('',406);
        }
        
        // by CPE
        @$cpe = $this->_request['cpe'];        
        if(isset($cpe))
        {
            $cmd = "LMIP $cpe";
            $result = $this->softgredCmd($cmd, $cpe);
            $this->response($this->json($result), 200);
        }

        // by UE
        @$ue = $this->_request['ue'];
        if(isset($ue))
        {
            $cmd = "GTMC $ue";
            $result = $this->softgredCmd($cmd, $ue);
            $this->response($this->json($result), 200);
        }
        
        // If invalid inputs "Bad Request" status message and reason
        $error = array('status' => "Failed", "msg" => "Invalid params, expected ue=mac-of-ue or cpe=ip-of-cpe");
        $this->response($this->json($error), 400);
    }

    /**
     *    Encode array into JSON
     */
    private function json($data)
    {
        if(is_array($data))
        {
            return json_encode($data);
        }
    }
}

// Initiiate Library
$api = new API;
$api->processApi();
?>

