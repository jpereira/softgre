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

    pool();

    function pool() {
      $timeout(function() {
        refreshInterfacestatus();
        refreshLogs();
        pool();
      }, 3000);
    }

    function refreshLogs() {
      $http.get('http://gre.oiwifi.com.br/softgre/index.php?action=log&ajax=1&lastsize='+ lastSize + '&grep=' + grep + '&invert=' + invert).then(function(response){
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

    function refreshInterfacestatus() {
      $http.get('http://gre.oiwifi.com.br/softgre/api/tunnels').then(function(response){
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
  });
