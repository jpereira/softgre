all:
	ncu -u
	npm install --force
	npm audit fix --force
	bower install

devel:
	grunt serve

release:
	grunt build

depends:
	sudo npm install -g gifsicle
	sudo npm install -g grunt-contrib-imagemin
	sudo npm install -g bower
	sudo npm install -g grunt-cli

