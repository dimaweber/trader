alter table currencies add unique (name);
alter table order_status add unique(status);
update orders set order_id=-order_id where order_id < 0;
alter table orders ADD COLUMN modified TIMESTAMP NOT NULL ON UPDATE CURRENT_TIMESTAMP;
alter table orders ADD COLUMN created TIMESTAMP NOT NULL;
insert into version(major, minor) values (2, 1);
