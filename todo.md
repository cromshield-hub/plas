나는 C++17 기반으로 다양한 인터페이스를 정의하고, 그것을 지원하는 HW를 구현해서 상위 어플리케이션에게 기능을 제공하는 라이브러리를 만들려고 해.

project 이름 : plas (platform library across systems)

스타일은 아래를 따라줘
 - 폴더명, 파일명: snake 방식
 - class 이름, 함수이름: Camel
 - 변수 이름: snake, 클래스 멤버 변수는 _로 끝나게 해줘.

프로젝트의 구성은 아래를 고려해줘
 - base 혹은 core : 프로젝트 전반의 공통요소. type, error, unit, 혹은 공통적인 알고리즘.
 - log : 로그 기능을 전반에 제공하지. spdlog, cpplog등 다양한 것을 backend로 쓸수있어.이건 하드코딩되는것이고, runtime 변경은 안돼. 로그 파일은 내부적으로 지정된 용량 단위로 쪼개지며, 사용자는 현재 파일명을 알수있어야해.
 - config : json 혹은 yaml으로 config 제공. 해당 pc에 연결된 장치리스트를 nickname과 uri 같은주소, 기타 넘겨주고 싶은 arg를 담고있어.
 - backend/interface : i2c, i3c, serial, uart 등의 정의. 상위 어플리케이션은 이 정의를 이용해 기능 사용. 인스턴스의 생애 주기 관리도 가능하면 좋을듯해. open, close, init 등
    - i2c, i3c, serial, uart
    - power control
    - ssd gpio (perst, clock request, dualport en 등 ssd관련된 gpio 기능)
 - backend/driver : hw 구현을 담당하는거야. config를 읽어서 이 pc와 app instance에서 사용할 hw list와 interface list를 사전 구성할 수 있어야지. 다만 실제 handle open 과 같은 행동은 사용자의 요청으로 할거야.
    - aardvark, ft4222h: I2C를 지원해.
    - PMU3.0, PMU 4.0: power management unit. power control과 ssd gpio 기능 지원

 - xpmu: backend의 내용을 기반으로 pmu관련 기능을 묶은 2차 library 및 cli 구성. (이건 추후 구현 예정)
 - xsideband: i2c, i3c, doc 등 다양한 transport를 이용하여 sideband를 제공하는 library와 cli 구성 (이건 추후 구현 예정)


rpm 구성은 boost를 참고해서, 전체를 깔거나, 일부만 깔수도 있어야해.
 - plas: 모든 기능을 설치
 - plas-core or plas-base : 기본 기능 설치
 - plas-xpmu: xpmu와 의존성 설치
 - plas-xsideband: xpmu와 의존성 설치