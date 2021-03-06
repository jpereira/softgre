<?php
/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesse General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU Lesse General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesse General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
 */
class REST {
    public $_content_type = "application/json";
    private $_method = null;
    private $_func = null;
    private $_args = array();
    private $_code = 200;
    
    public function __construct()
    {
        $this->inputs();
    }
    
    public function get_referer()
    {
        return $_SERVER['HTTP_REFERER'];
    }
    
    public function response($data, $status)
    {
        $this->_code = ($status)? $status : 200;
        $this->set_headers();
        echo $data;
        exit;
    }
    
    public function get_func()
    {
        return $this->_func;
    }

    public function get_args()
    {
        return $this->_args;
    }

    private function get_status_message()
    {
        $status = array(
                    100 => 'Continue',  
                    101 => 'Switching Protocols',  
                    200 => 'OK',
                    201 => 'Created',  
                    202 => 'Accepted',  
                    203 => 'Non-Authoritative Information',  
                    204 => 'No Content',  
                    205 => 'Reset Content',  
                    206 => 'Partial Content',  
                    300 => 'Multiple Choices',  
                    301 => 'Moved Permanently',  
                    302 => 'Found',  
                    303 => 'See Other',  
                    304 => 'Not Modified',  
                    305 => 'Use Proxy',  
                    306 => '(Unused)',  
                    307 => 'Temporary Redirect',  
                    400 => 'Bad Request',  
                    401 => 'Unauthorized',  
                    402 => 'Payment Required',  
                    403 => 'Forbidden',  
                    404 => 'Not Found',  
                    405 => 'Method Not Allowed',  
                    406 => 'Not Acceptable',  
                    407 => 'Proxy Authentication Required',  
                    408 => 'Request Timeout',  
                    409 => 'Conflict',  
                    410 => 'Gone',  
                    411 => 'Length Required',  
                    412 => 'Precondition Failed',  
                    413 => 'Request Entity Too Large',  
                    414 => 'Request-URI Too Long',  
                    415 => 'Unsupported Media Type',  
                    416 => 'Requested Range Not Satisfiable',  
                    417 => 'Expectation Failed',  
                    500 => 'Internal Server Error',  
                    501 => 'Not Implemented',  
                    502 => 'Bad Gateway',  
                    503 => 'Service Unavailable',  
                    504 => 'Gateway Timeout',  
                    505 => 'HTTP Version Not Supported');
        return ($status[$this->_code])?$status[$this->_code]:$status[500];
    }
    
    public function get_request_method()
    {
        return $_SERVER['REQUEST_METHOD'];
    }
    
    private function inputs()
    {
        switch($this->get_request_method())
        {
            case "POST":
                $this->_func = strip_tags($_POST['_func']);
                $this->_args = $this->cleanInputs($_POST['_args']);
                break;
            case "GET":
            case "DELETE":
                $this->_func = strip_tags($_GET['_func']);
                $this->_args = $this->cleanInputs($_GET['_args']);
                break;
            case "PUT":
                parse_str(file_get_contents("php://input"), $this->_args);
                $this->_args = $this->cleanInputs($this->_args);
                break;
            default:
                $this->response('', 406);
                break;
        }
    }        
    
    private function cleanInputs($_args)
    {
        $clean_input = array();

        if (is_array($_args))
            $this->response("erro: need to declare \$_GET['_args'] with string, not array!",405);

        $arr = array_chunk(explode('/', trim($_args,'/')), 2);
        foreach($arr as $pair)
        {
            $key = $pair[0];
            $val = isset($pair[1]) ? $pair[1] : null;

            if(get_magic_quotes_gpc())
            {
                $val = strip_tags($val);
            }

            $clean_input[$key] = $val;
        }
        //debug_array($clean_input);
        return $clean_input;
    }        
    
    private function set_headers()
    {
        header("HTTP/1.1 ".$this->_code." ".$this->get_status_message());
        header("Content-Type:".$this->_content_type);
        header("Access-Control-Allow-Origin: *");
        header("Access-Control-Allow-Methods: POST, GET, OPTIONS");
    }
}    
?>
