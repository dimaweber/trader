alter table currencies add unique (name);
alter table order_status add unique(status);
alter table orders ADD COLUMN modified TIMESTAMP NOT NULL ON UPDATE CURRENT_TIMESTAMP;
alter table orders ADD COLUMN created TIMESTAMP NOT NULL;
insert into version(major, minor) values (2, 1);
