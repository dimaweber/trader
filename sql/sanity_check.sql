-- orders from complete rounds with improper status
select o.order_id from orders o left join rounds r on r.round_id=o.round_id where r.reason='sell' and ( o.status < 1 or o.status > 3);

-- canceled orders with amount and start_amount mismatch
select o.order_id from orders where status=2 and amount <> start_amount;

-- complete orders with non-zero amount
select o.order_id from orders where status=1 and amount <> 0;

-- rounds with goods income mismatch orders income
select r.round_id, truncate(r.g_in, 5) as rounds_g_in, truncate(sum(start_amount - amount)*(1-s.comission),5) as orders_g_in from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id where r.reason='sell' and o.│··
type='buy' group by r.round_id having rounds_g_in <> orders_g_in;

-- rounds with goods outcome mismatch orders outcome
select r.round_id, truncate(r.g_out, 5) as rounds_g_out, truncate(SUM(o.start_amount-o.amount),5)  as orders_g_out from orders o left join rounds r on r.round_id=o.round_id where r.reason='sell' and o.type='sell' group by r.round_id having rounds_g│··
_out <> orders_g_out;

-- rounds with goods income / outcome mismatch
select r.round_id from rounds  r where truncate(r.g_in,5) <> truncate(r.g_out,5);

-- rounds with negative outcome
select r.round_id from rounds r where r.income < 0;
