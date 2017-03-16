-- create table if not exists order_status (status_id integer primary key, status char(16));
insert into order_status (status_id, status) values (-1, 'check'), (0, 'active'), (1, 'done'), (2, 'cancel'), (3, 'partially done');
-- create table if not exists  currencies (currency_id integer primary key auto_increment, name char(3));
insert into currencies (name) values ('btc'),('cnh'),('dsh'),('eth'),('eur'),('ftc'),('gbp'),('ltc'),('nmc'),('nvc'),('ppc'),('rur'),('trc'),('usd'),('xpm');

alter table settings add foreign key (secret_id) references secrets(id);

update rounds set reason='done' where reason='sell';
alter table rounds modify reason enum('active', 'done') not null default 'active';
alter table rounds add foreign key (settings_id) references settings(id);

alter table orders modify type enum('buy', 'sell') not null default 'buy';
alter table orders change status status_id integer not null default 0;
alter table orders drop settings_id;
alter table orders drop backed_up;
delete from orders where round_id=0;
alter table orders add foreign key (round_id) references rounds(round_id);
alter table orders add foreign key (status_id) references order_status(status_id);

alter table transactions modify order_id integer not null;
alter table transactions add foreign key (secret_id) references secrets(id);

alter table dep add foreign key (secret_id) references secrets(id);

insert into version(major, minor) values (2, 0);
