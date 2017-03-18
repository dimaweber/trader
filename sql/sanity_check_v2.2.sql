-- check database version is 2.2
select * from (select major,minor from version order by id desc limit 1) A where major<>2 or minor <> 2;
-- orders with negative order_id
select o.order_id from orders o where o.order_id <= 0;
-- orders from complete rounds with improper status
select o.order_id from orders o left join rounds r on r.round_id=o.round_id where r.reason='done' and ( o.status_id < 1 or o.status_id > 4);
-- canceled orders with amount and start_amount mismatch
select o.order_id from orders o where o.status_id=2 and o.amount <> o.start_amount;
-- complete orders with non-zero amount
select o.order_id from orders o where o.status_id in (1, 4) and o.amount <> 0;
-- orders with create/modify time not within from rounds start/end time
select o.order_id from orders o left join rounds r on r.round_id=o.round_id where ((o.created NOT BETWEEN r.start_time and r.end_time ) or (o.modified NOT BETWEEN r.start_time and r.end_time)) and r.reason = 'done';
-- orders with created timestamp > modified
select order_id from orders where created > modified;
-- orders with zero start_amount / rate
select order_id from orders where start_amount = 0 or rate = 0;
-- non instant orders not in transactions log
select o.order_id from transactions t right join orders o on o.order_id=t.order_id where t.id is null and o.status_id <> 4;
-- partially done orders with start_amount == amount or zero amount
select order_id from orders where status_id=3 and (start_amount = amount or amount=0);
-- rounds with goods income mismatch orders goods income
select r.round_id, truncate(r.g_in, 5) as rounds_g_in, truncate(sum(start_amount - amount)*(1-s.comission),5) as orders_g_in from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id where r.reason='done' and o.type='buy' group by r.round_id having abs(rounds_g_in - orders_g_in) > .00001;
-- rounds with goods outcome mismatch orders goods outcome
select r.round_id, truncate(r.g_out, 5) as rounds_g_out, truncate(SUM(o.start_amount-o.amount),5)  as orders_g_out from orders o left join rounds r on r.round_id=o.round_id where r.reason='done' and o.type='sell' group by r.round_id having rounds_g_out <> orders_g_out;
-- rounds with currency income mismatch orders currency income
select r.round_id, truncate(sum((o.start_amount - o.amount)*o.rate)*(1-s.comission), 3) as orders_c_in, truncate(r.c_in, 3) as rounds_c_in from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id where status_id <> 2 and r.reason='done' and o.type='sell' group by r.round_id having orders_c_in <> rounds_c_in;
-- rounds with goods income / outcome mismatch
select r.round_id, r.g_in, r.g_out from rounds  r where abs(truncate(r.g_in,5) - truncate(r.g_out,5)) > .00001;
-- rounds with negative outcome
select r.round_id, r.income from rounds r where r.income < 0;
-- rounds with end_time and reason mismatch
select r.round_id from rounds r where (r.end_time is not null and r.reason <> 'done') or (r.end_time is null and r.reason <> 'active');
-- rounds with no done / partially done orders (might be false positive)
select r.round_id, count(*) as cnt from orders o left join rounds r on r.round_id=o.round_id where o.status_id <> 2 group by r.round_id having cnt=0;
-- rounds with no orders at all (might temporary be false positive)
select r.round_id from rounds r left join orders o on o.round_id=r.round_id where  o.order_id is null;
-- rounds with only sell orders, no buy orders
select S.round_id from (select o.round_id from orders o where o.type='sell' and o.status_id<>2 group by o.round_id) S left join (select o2.round_id from orders o2 where o2.type='buy' and o2.status_id<>2 group by o2.round_id) B on B.round_id = S.round_id where B.round_id is NULL;
-- done rounds with buy orders only, no sell orders
select B.round_id from (select o.round_id from orders o where o.type='sell' and o.status_id<>2 group by o.round_id) S right join (select o2.round_id from orders o2 left join rounds r2 on r2.round_id=o2.round_id where r2.reason='done' and o2.type='buy' and o2.status_id<>2 group by o2.round_id) B on B.round_id = S.round_id where S.round_id is NULL;
-- rounds with zero dep_usage
select r.round_id from rounds r where r.dep_usage=0;
-- round orders goods in/out mismatch
select B.round_id, B.g_in * (1-B.comission), S.g_out from (select o1.round_id, sum(o1.start_amount-o1.amount) as g_in, s.comission from orders o1 left join rounds r on r.round_id=o1.round_id left join settings s on r.settings_id=s.id where o1.type='buy' and r.reason='done' group by o1.round_id) B left join (select o2.round_id, sum(o2.start_amount-o2.amount) as g_out from orders o2 where o2.type='sell' group by o2.round_id) S on B.round_id=S.round_id where abs(B.g_in * (1-B.comission) - S.g_out) > .0001;
-- active rounds with non zero g/c in/out
select r.round_id from rounds r where r.c_in + r.c_out + r.g_in + r.g_out <> 0 and r.reason = 'active';
-- done rounds with more then 1 executed sell
select round_id, count(*) as cnt from orders where type='sell' and status_id in (1,4) group by round_id having cnt > 1;
-- rounds with sell order and zero done or partially done buy orders
select * from (select round_id, count(*) as cnt from orders where type='sell' group by round_id) S left join (select round_id, count(*) as cnt from orders where type='buy' and status_id in (1, 3, 4) group by round_id) B on B.round_id=S.round_id where B.cnt=0 and S.cnt>0;
-- rounds with currency spend higher then dep_usage
select round_id from rounds where c_out > dep_usage;
-- settings with degative deposit
select * from settings where dep < 0;
-- enabled settings with no active rounds
select s.id from settings s left join rounds r on s.id=r.settings_id where r.round_id is null and s.enabled=true;
-- settings with more then one active round
select s.id, count(r.round_id) as active_round_count from settings s left join rounds r on r.settings_id = s.id where s.enabled = 1 and r.reason='active' group by s.id having active_round_count <> 1;
