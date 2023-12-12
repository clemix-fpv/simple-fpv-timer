// SPDX-License-Identifier: GPL-3.0+

var tab_name = "laps";
var gateway = `ws://${window.location.hostname}:${window.location.port}/ws/rssi`;
var websocket;

function init_websocket() {
    console.log('Trying to open a WebSocket connection...' + gateway);
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
}
function onOpen(event) {
    on_rssi_update({"CONNECTION": "OPEN"});
}
function onClose(event) {
    on_rssi_update({"CONNECTION": "CLOSED"});
    if (tab_name == "debug"){
        setTimeout(init_websocket, 2000);
    }
}
function onMessage(event) {
    try {
        json = JSON.parse(event.data);
        on_rssi_update(json);
        graph_update(json);
        
    } catch (e) {
        return console.error(e); // error in the above string (in this case, yes)!
    }
}

function on_rssi_update(j)
{
    let dbg = $('#debug');
    $('#dbg_rssi_card').remove();
    t = $('<table>')
    for (let key in j){
        let value = j[key];
        t.append(`<tr><td>${key}</td><td>${value}</td></tr>`);
    }
    dbg.prepend(build_card('RSSI:', t, "dbg_rssi_card"));

    if (tab_name != "debug"){
        websocket.close();
    }
}

function simple_api_call(cmd)
{
    $.post('/api/v1/'+cmd, {},
        function (ret_data){
        }
    );
}

function format_ms(ms)
{
    var sec_num = parseInt(ms, 10);
    var hours   = Math.floor(sec_num / 3600 / 1000);
    var minutes = Math.floor((sec_num - (hours * 3600 * 1000)) / 60 / 1000);
    var seconds = Math.floor((sec_num - (hours * 3600 * 1000)  - (minutes * 60 * 1000)) / 1000);
    var ms = sec_num - (hours * 3600 * 1000) - (minutes * 60 * 1000) - seconds * 1000;

    var t = [[hours, "h"], [minutes,"m"], [seconds,"s"], [ms,"ms"]];
    var p = false;
    var ret = "";
    for (i=0; i < t.length; i++){
        if (t[i][0] > 0 || p) {
            ret += t[i][0] + t[i][1] + " ";
            p=true;
        }
    }
    return ret;
}

function notify(success, msg)
{
    var e = $('#box_notify');
    if (success) {
        e.addClass("border-success bg-success");
        e.removeClass("border-danger bg-danger");
    } else {
        e.addClass("border-danger bg-danger");
        e.removeClass("border-success bg-success");
    }
    e.find('h5').html(msg);
    e.stop();
    e.css('top', 0);
    e.animate({top: -1 * e.height() - 5}, 1500);
}

function notify_response(msg, data) 
{
    if (data.status == "ok")
    {
        msg += " SUCCESSFUL";
        if (data.msg)
            msg += " - " + data.msg;
        notify(true, msg)
    } else {
        msg += " FAILED";
        if (data.msg)
            msg += " - " + data.msg;
        notify(false, msg);
    }
}

function build_freq(key, value)
{
    var h = '<div class="form-group" id="fg_settings_'+key+'">';
    h += '<fieldset>';
    h += '<label class="form-label">Frequenz ('+value+'MHz)</label><br/>';

    var channels =  [
        { freq: 5658, name: "R1 (5658)"  },
        { freq: 5695, name: "R2 (5695)"  },
        { freq: 5732, name: "R3 (5732)"  },
        { freq: 5769, name: "R4 (5769)"  },
        { freq: 5806, name: "R5 (5806)"  },
        { freq: 5843, name: "R6 (5843)"  },
        { freq: 5880, name: "R7 (5880)"  },
        { freq: 5917, name: "R8 (5917)"  },
    ];
    h += '<select name="'+key+'" class="custom-select custom-select-lg mb-3">';
    $.each(channels, function(i, o){
        var selected = (value == o.freq) ? "selected" : "";
        h += '<option value="'+o.freq+'" '+selected+' >'+o.name+'</option>';
    });
    h += '</select>';
    h += '</fieldset>';
    h += '</div>';

    return h;
}

function build_wifi_mode(key, value)
{
    var h = '<div class="form-group" id="fg_settings_'+key+'">';
    h += '<fieldset>';
    h += '<label class="form-label">Wifi Mode</label><br/>';

    var channels =  [
        { freq: 0, name: "Access Point"  },
        { freq: 1, name: "Station"  },
    ];
    h += '<select name="'+key+'" class="custom-select custom-select-lg mb-3">';
    $.each(channels, function(i, o){
        var selected = (value == o.freq) ? "selected" : "";
        h += '<option value="'+o.freq+'" '+selected+' >'+o.name+'</option>';
    });
    h += '</select>';
    h += '</fieldset>';
    h += '</div>';

    return h;
}


function build_common_setting(key, value)
{
    return $('<div class="form-group" id="fg_settings_'+key+'">' + 
        '<fieldset>'+
        '<label class="form-label" for="input_'+key+'">'+key+'</label>'+
        '<input class="form-control input_'+key+'" name="'+key+'" id="input_'+key+'" type="text" value="'+value+'">'+
        '</fieldset>'+
        '</div>');
}


function build_common_status(key, value)
{
    return  '<tr><td>'+key+'</td><td>'+value+'</td></tr>';
}

function build_common_status_players(key, players)
{
    var h = "";
    $.each ( players, function (i, p) {
        for(var key in p) {
            if (key == "laps" ) {
                h += '<table>';
                h += '<thead><tr><td>id</td><td>duration</td><td>rssi</td></tr></thead>';
                h += '<tbody>';
                $.each ( p.laps, function (i, v) {
                    h += '<tr><td>'+ v.id +'</td><td>'+ v.duration +'</td><td>'+v.rssi+'</td></tr>';
                });
                h += '</tbody>';
                h += '</table>';
            } else {
                h += "<div>" + key + ": " + p[key] + "</div>";
            }
        }
    });
    return build_common_status(key, h);
}


function build_settings(config)
{
    var s = $('#settings');
    s.empty();

    var form_groups = [
        { 
            name: "Player settings",
            elements: ["freq", "player_name"]
        },
        { 
            name: "Callibration/Meassurment",
            elements: ["rssi_peak", "rssi_filter", "rssi_offset_enter", "rssi_offset_leave", "calib_max_lap_count", "calib_min_rssi_peak"]
        },
        { 
            name: "WiFi",
            elements: ["wifi_mode", "ssid", "passphrase"]
        }
    ];

    form = $('<form class="form-horizontal" action="" id="form_settings" method="post"/>');
    var lg = $('<ul class="list-group">');
    for(var fg of form_groups){
        var lgi = $('<li class="list-group-item bg-gradient">');
        lgi.append('<h5 style="text-align: right" >'+fg.name+'</h5>');
        for(var elem of fg.elements) {
            var h = "";
            if (elem == "freq") {
                h = build_freq(elem, config[elem]);
            } else if (elem == "wifi_mode") {
                h = build_wifi_mode(elem, config[elem]);  
            } else {
                h = build_common_setting(elem, config[elem]);
            }
            lgi.append(h);
        }
        lg.append(lgi);
    }
    form.append(lg);
    form.append('<div class="d-flex justify-content-center"><button type="submit" class="btn btn-primary">Submit</button></div>');
    s.append(form);
    form.on( "submit", function( event ) {
        event.preventDefault();
        let mydata = $(this).serialize();
        $.ajax({
            url: '/api/v1/settings',
            type: 'POST',
            data: $(this).serialize()
        }).always(function(data){
            notify_response("Configuration saved", data);
        });

    });

}

function update_settings()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            build_settings(data.config);
        });
}

var round_idx = {};
var sound;
function on_new_lap(lap)
{
    if($('.btn_enable_sound').data('sound-enabled')){
        sound.play();
    }
}

function update_home()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            var stats = data.status;

            if (stats.in_calib_mode) {
                $('#b_cali').hide();
                $('#p_cali').show();
                var p_bar = $('div#p_cali > div');
                var w = Math.floor(stats.in_calib_lap_count / data.config.calib_max_lap_count * 100);
                w = w < 10 ? 10: w;
                p_bar.css("width", w + "%");
                p_bar.attr('aria-valuemin', 0);
                p_bar.attr('aria-valuemax', data.config.calib_max_lap_count);
                p_bar.attr('aria-valuenow', stats.in_calib_lap_count);
                p_bar.text(stats.in_calib_lap_count + "/" + data.config.calib_max_lap_count + "laps"+
                    " rssi-peak:" + stats.rssi_peak);
            } else {
                $('#b_cali').show();
                $('#p_cali').hide();
            }
            var tb = $('#d_labs tbody');
            tb.empty();
            var rows = [];
            $.each(stats.players, function(i,p){

                var fastes = 99999999999999;
                $.each(p.laps, function (i, v) {
                    var t = v.duration;
                    if (t == 0) return;
                    if (fastes > t)
                        fastes = t;
                });
                if (p.laps.length > 0) {
                    var last_id = p.laps[p.laps.length - 1].id;
                    if (!(p.name in round_idx) || round_idx[p.name] != last_id) {
                        on_new_lap(p.laps[p.laps.length - 1])
                        round_idx[p.name] = last_id;
                    }
                }

                $.each(p.laps, function (i, v) {
                    if (v.duration == 0) return;
                    v.fastes = v.duration == fastes;
                    v.player = p.name;
                    rows.push(v);
                });
            });
            rows.sort(function(a,b){
                return a.abs_time < b.abs_time;
            });

            $.each(rows, function (i, v) {
                var c = 'table-light';
                if (v.fastes) {
                    c = 'table-success';
                }
                var h = '<tr class="' + c + '">';
                h += '<td>' + v.player + '</td>';
                h += '<td>' + v.id + '</td>';
                h += '<td>' + format_ms(v.duration) + '</td>';
                h += '<td>' + v.rssi + '</td>';
                h += '</tr>';
                tb.append($(h));
            });

        });

    if (tab_name == 'home') {
        setTimeout(update_home, 2000);
    }
}

function update_players()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            var stats = data.status;

            var cards = [];
            $.each(stats.players, function(i,p){

                var fastes = null;
                var time_sum = 0;
                $.each(p.laps, function (i, v) {
                    if (!fastes){
                        fastes = v;
                    } else if(fastes.duration > v.duration) {
                        fastes = v;
                    }
                    time_sum += v.duration;
                });

                if (p.laps.length > 0) {
                    var last_id = p.laps[p.laps.length - 1].id;
                    if (!(p.name in round_idx) || round_idx[p.name] != last_id) {
                        on_new_lap(p.laps[p.laps.length - 1])
                        round_idx[p.name] = last_id;
                    }
                }

                cards.push({
                    name: p.name,
                    ipaddr: p.ipaddr,
                    time: time_sum > 0 ? time_sum : 0,
                    lap: p.laps.length > 0 ? p.laps[p.laps.length-1].id : 0,
                    fastes: fastes
                });
            });

            cards.sort(function(a,b){
                var lap_diff = b.lap - a.lap;
                if (lap_diff != 0)
                    return lap_diff;

                t_a = a.time > 0 ? a.time : Number.MAX_SAFE_INTEGER;
                t_b = b.time > 0 ? b.time : Number.MAX_SAFE_INTEGER;

                return t_a == t_b ? 0 : (t_a > t_b)? 1 : -1;
            });

            $.each(cards, function (i, v) {
                v.rank = i + 1;
            });

            cards.sort(function(a,b){
                return a.name.localeCompare(b.name);
            });

            $('#player_cards').empty();
            $.each(cards, function (i, v) {
                card = $('<div class="card" style="margin: 5px;">');
                var medal = "";
                medal = (v.rank == 1)? "&#127942;"/*"&#129351;"*/ : 
                    (v.rank == 2)? "&#129352;" : 
                    (v.rank == 3)? "&#129353;" : 
                    v.rank + ".";
                var btn_edit = (v.ipaddr)? '<a href="http://'+v.ipaddr+'" class="btn btn-secondary btn-sm" style="float:right" role="button" >&#9881;</a>':'';
                card.append('<h5 class="card-header">'+ medal +" "+ v.name + btn_edit +'</h5>');
                table = $('<table>');
                table.append('<tr><td class="card-text" >Lap:</td><td class="card-text">'+v.lap+'</td></tr>');
                table.append('<tr><td class="card-text" >Time:</td><td class="card-text">'+ (v.time > 0 ? format_ms(v.time) : 0) +'</td></tr>');
                fastes = "";
                if (v.fastes) {
                    fastes = format_ms(v.fastes.duration) + ' <small class="text-muted">(Lap ' + v.fastes.id + ')</small>'; 
                }
                table.append('<tr><td class="card-text">Fastes:</td><td class="card-text">'+ fastes +'</td></tr>');
                card.append(table);
                $('#player_cards').append(card);
            });
        });

    if (tab_name == 'players') {
        setTimeout(update_players, 1000);
    }
}

function build_card(title, content, id)
{
    card = $('<div class="card border-primary mb-3" style="max-width: 20rem;">')
    if (typeof id !== 'undefined'){
        card.attr('id', id);
    }
    card_header = $('<div class="card-header">');
    card_header.html(title);
    card_body = $('<div class="card-body">');
    card_body.html(content);
    card.append(card_header);
    card.append(card_body);
    return card;
}

let uplot = null;
function graph(id)
{
    let xs = [1,2,3,4,5,6,7,];
    let vals = [-10,-9,-8,-7];

    let data = [
        new Float32Array(xs),
        new Float32Array(xs.map((t, i) => vals[Math.floor(Math.random() * vals.length)])),
    ];

    data = [
        [],
        [],
        [],
    ];

    let e = $('#' + id);
    const opts = {
        width: e.innerWidth(),
        height: 600,
        title: "RSSI",
        scales: {
            x: {
                time: false,
            },
            y: {
                auto:false,
                range: [100, 1200],
            }
        },
        series: [
            {
                label: "millis",
                value: (u, v) => v == null ? null : (v - u._data[0][0]) + "ms",
            },
            {
                stroke: "red",
                label: "rssi",
            },
            {
                stroke: "green",
                label: "rssi-smooth",
            },
        ],
    };

    uplot = new uPlot(opts, data, document.getElementById(id));
}

let collect_rssi = [];
let laps = [];
let color = [255, 165, 0];
function graph_update(json)
{
    let data = uplot._data;
    let timeduration = 10000;

    rssi_idx = data.length - 2;

    if (json.i){
        collect_rssi.push(json)
    } else {
        if (collect_rssi.length > 10) {
            c = "rgba("+Math.floor(Math.random() * 255) +","+Math.floor(Math.random() * 255)+","+Math.floor(Math.random() * 255)+", 0.1)"
            uplot.addSeries({
                stroke: c,
				fill: c,
                label: "Lap " + rssi_idx
			}, rssi_idx);
            data.splice(rssi_idx, 0, [] );
            laps.push({last_idx: 0, idx: rssi_idx, data: collect_rssi})

/*            $.each(collect_rssi, function(i,o){
                data[rssi_idx].push(o.s);
            });
            */
            collect_rssi = new Array();
        }
    }

    data[0].push(json.t);
    data[rssi_idx].push(json.r);
    data[rssi_idx + 1].push(json.s);

    let older_as = json.t - timeduration;
    while(data[0][0] < older_as){
        data[0].shift();
        data[rssi_idx].shift();
        data[rssi_idx + 1].shift();
    }
    $.each(laps, function (i,o){
        let arr = data[o.idx];
        let start_idx = Math.floor((data[0].length - o.data.length) / 2);
        if (o.last_idx == start_idx) return;
        o.last_idx = start_idx;
        arr.fill(0,0, start_idx);
        $.each(o.data, function(j,u){
            arr[j+start_idx] = u.r;
        });
    });
   
    uplot.setData(data);
}

function update_debug()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            let dbg = $('#debug');
            dbg.empty();

            /* build status table */
            let status_table = $('<table class="table">');
            status_table.append('<thead><tr><th>name</th><th>value</th></tr></thead>');
            stats = data.status;
            for(let key in stats) {
                let h;
                if (key == 'players') {
                    h = build_common_status_players(key, stats[key]);
                } else {
                    h = build_common_status(key, stats[key]);
                }

                status_table.append(h);
            }

            dbg.append(build_card('Status:', status_table));

            dbg.append(build_card('Graph:', "<div id='id_graph'/>"));
            graph("id_graph");
        });

    if (tab_name == 'debug') {
        //setTimeout(update_debug, 500);    
    }
}

$('.nav a[href$=settings]').on('click', function(e){
    tab_name = "settings";
    update_settings();
});

$('.nav a[href$=players]').on('click', function(e){
    tab_name = "players";
    update_players();
});

$('.nav a[href$=laps]').on('click', function(e){
    tab_name = "laps";
    update_home();
});

$('.nav a[href$=debug]').on('click', function(e){
    tab_name = "debug";
    update_debug();
    init_websocket();
});

$('.btn_enable_sound').click(function(){
    var enabled = ! $(this).data('sound-enabled');
    $('.btn_enable_sound').data('sound-enabled', enabled)
    $('.btn_enable_sound').html(enabled ? '&#x1F50A;' : '&#128263;');
    $('.btn_enable_sound').attr('title', "Disable sounds");
});

$(function () {
    update_home();
    $('[data-toggle="tooltip"]').tooltip({trigger: 'hover'});
    Howler.autoUnlock = false;
    sound = new Howl({
        src: ['round.ogg']
    });

});



