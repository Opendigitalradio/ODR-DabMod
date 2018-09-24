//   Copyright (C) 2018
//   Matthias P. Braendli, matthias.braendli@mpb.li
//
//    http://www.opendigitalradio.org
//
//   This file is part of ODR-DabMod.
//
//   ODR-DabMod is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as
//   published by the Free Software Foundation, either version 3 of the
//   License, or (at your option) any later version.
//
//   ODR-DabMod is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.

function requestStatus(callback) {
    $('#rctable > tbody').empty();

    doApiRequestGET("/api/rc_parameters", function(data) {
        $.each( data, function( key1, controllable ) {
            $.each( controllable, function( key2, param ) {
                $('#rctable > tbody:last').append(
                    '<tr><td>'+key1+'.'+key2+'</td>'+
                    '<td>'+param['value']+'</td>'+
                    '<td>'+param['help']+'</td></tr>');
            });
        });
    });
}


$(function(){
    requestStatus();

    $('#refresh').click(function() {
        requestStatus();
        $.gritter.add({
                    title: 'Services status refresh',
                    image: '/fonts/accept.png',
                    text: 'Ok',
                });
    });
});


// ToolTip init
$(function(){
    $('[data-toggle="tooltip"]').tooltip();
});
