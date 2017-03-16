-- check database version is 2.1
select * from (select major,minor from version order by id desc limit 1) A where major<>2 or minor <> 1;

-- orders from complete rounds with improper status
select o.order_id from orders o left join rounds r on r.round_id=o.round_id where r.reason='done' and ( o.status_id < 1 or o.status_id > 3);

-- canceled orders with amount and start_amount mismatch
select o.order_id from orders o where o.status_id=2 and o.amount <> o.start_amount;

-- complete orders with non-zero amount
select o.order_id from orders o where o.status_id=1 and o.amount <> 0;

-- rounds with goods income mismatch orders goods income
select r.round_id, truncate(r.g_in, 5) as rounds_g_in, truncate(sum(start_amount - amount)*(1-s.comission),5) as orders_g_in from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id where r.reason='done' and o.type='buy' group by r.round_id having rounds_g_in <> orders_g_in;

-- rounds with goods outcome mismatch orders goods outcome
select r.round_id, truncate(r.g_out, 5) as rounds_g_out, truncate(SUM(o.start_amount-o.amount),5)  as orders_g_out from orders o left join rounds r on r.round_id=o.round_id where r.reason='done' and o.type='sell' group by r.round_id having rounds_g_out <> orders_g_out;

-- rounds with currency income mismatch orders currency income
select r.round_id, truncate(sum((o.start_amount - o.amount)*o.rate)*(1-s.comission), 3) as orders_c_in, truncate(r.c_in, 3) as rounds_c_in from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id where status_id <> 2 and r.reason='done' and o.type='sell' group by r.round_id having orders_c_in <> rounds_c_in;

-- rounds with goods income / outcome mismatch
select r.round_id, r.g_in, r.g_out from rounds  r where truncate(r.g_in,5) <> truncate(r.g_out,5);

-- rounds with negative outcome
select r.round_id, r.income from rounds r where r.income < 0;

-- settings with more then one active round
select s.id, count(r.round_id) as active_round_count from settings s left join rounds r on r.settings_id = s.id where s.enabled = 1 and r.reason='active' group by s.id having active_round_count <> 1;

-- rounds with end_time and reason mismatch
select r.round_id from rounds r where (r.end_time is not null and r.reason <> 'done') or (r.end_time is null and r.reason <> 'active');

-- orders with negative order_id
select o.order_id from orders o where o.order_id <= 0;

-- orders with create/modify time different from rounds start/end time
select o.order_id from orders o left join rounds r on r.round_id=o.round_id where ((o.created NOT BETWEEN r.start_time and r.end_time ) or (o.modified NOT BETWEEN r.start_time and r.end_time)) and r.reason = 'done';

-- rounds that start buying higher then previous round sold: NEED FIX -- different settings mess!!!!
-- select CURR.round_id, PREV.round_id, CURR.buy, PREV.sell from (select o.round_id as round_id, max(o.rate) as buy from orders o where status_id=1 and type='buy' group by o.round_id) CURR left join (select o.round_id as round_id, max(o.rate) as sell from orders o where status_id=1 and type='sell' group by o.round_id) PREV on PREV.round_id+1=CURR.round_id where CURR.round_id is not null and CURR.buy >= PREV.sell;
