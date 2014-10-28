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

'use strict';

angular.module('greStatusApp')
  .controller('MainCtrl', function ($scope, $http, $timeout) {

    var host_gre = "http://" + window.location.host;
    var lastSize = 0; 
  	var grep = "";
    var invert = 0;
    var itensPerPage = 20;

    $scope.logs = {
      currentPage: 'last',
      pages: [[]]
    };

    $scope.lastLogs = [];
    $scope.source = [];

    $scope.pageToRender = function() {
      if($scope.logs.currentPage === 'last') {
        return $scope.lastLogs;
      } else {
        return $scope.logs.pages[$scope.logs.currentPage];
      }
    }

    $scope.goToPage = function(i) {
      $scope.logs.currentPage = i;
    }

    $scope.canShowPage = function(i) {
      return i < $scope.maxPageToShow() && i > $scope.minPageToShow();
    }

    $scope.minPageToShow = function() {
      if($scope.logs.currentPage === 'last') {
        return $scope.logs.pages.length - 8;
      } else {
        return $scope.logs.currentPage - 3;
      }
    };

    $scope.maxPageToShow = function() {
      if($scope.logs.currentPage === 'last') {
        return $scope.logs.pages.length - 1;
      } else {
        return $scope.logs.currentPage + 3;
      }
    }

    function refreshLogs() {
      $http.get(host_gre+'/softgre/wrapper.php?action=log&ajax=1&lastsize='+ lastSize + '&grep=' + grep + '&invert=' + invert).then(function(response){
        lastSize = response.data.size;

        var lastPage = $scope.logs.pages[$scope.logs.pages.length - 1];

        angular.forEach(response.data.data, function(value) {
          lastPage.push(value);

          if(lastPage.length % itensPerPage === 0) {
            $scope.logs.pages.push([]);
          }
        });

        $scope.source = $scope.source.concat(response.data.data);
        $scope.lastLogs = $scope.source.slice($scope.source.length - 1 - itensPerPage, $scope.source.length - 1);
      });
    }

    function refreshServerStatus() {
      $http.get(host_gre+'/softgre/api/status').then(function(response){
        if (response.data.status == 'OK') {
          var temp = response.data.body.split(';');
          $scope.sys_status = 'OK';
          $scope.attr_status = {
              os_name:      temp[0],
              os_version:   temp[1],
              os_arch:      temp[2],
              sys_uptime:   moment.duration(temp[3]*1000).humanize(),
              sgre_version: temp[4],
              sgre_started: new Date(parseInt(temp[5])*1000)
          };
        } else {
          $scope.sys_status = 'NOK';
          $scope.attr_status = undefined;
        }
      });
    }

    function refreshInterfacestatus() {
      $http.get(host_gre+'/softgre/api/tunnels').then(function(response){
        if(response.data.status == 'OK') {
          $scope.status = 'OK';
          $scope.attrs = response.data.body.split(';');
          var result = [];
          angular.forEach($scope.attrs, function(value){
            if(value === '') {
              return;
            }
            var temp = value.split('@');
            value = {
              ip: temp[0],
              interface: temp[1]
            };
            result.push(value);
          });
          $scope.attrs = result;
        } else {
          $scope.status = 'NOK';
          $scope.attrs = undefined;
        }
      });
    }

    function poolStatus() {
      $timeout(function() {
        refreshServerStatus();
        poolStatus();
      }, 5000);
    }

    function pool() {
      $timeout(function() {
        refreshInterfacestatus();
        refreshLogs();
        pool();
      }, 3000);
    }

    poolStatus();
    pool();
  });
