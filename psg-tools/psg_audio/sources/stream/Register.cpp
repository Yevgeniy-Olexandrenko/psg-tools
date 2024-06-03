#include "Register.h"

const Register::Value Register::t_fine   [] { A_Fine,    B_Fine,    C_Fine    };
const Register::Value Register::t_coarse [] { A_Coarse,  B_Coarse,  C_Coarse  };
const Register::Value Register::t_duty   [] { A_Duty,    B_Duty,    C_Duty    };
const Register::Value Register::volume   [] { A_Volume,  B_Volume,  C_Volume  };
const Register::Value Register::e_fine   [] { EA_Fine,   EB_Fine,   EC_Fine   };
const Register::Value Register::e_coarse [] { EA_Coarse, EB_Coarse, EC_Coarse };
const Register::Value Register::e_shape  [] { EA_Shape,  EB_Shape,  EC_Shape  };

const Register::Period Register::t_period [] { Period::A,  Period::B,  Period::C  };
const Register::Period Register::e_period [] { Period::EA, Period::EB, Period::EC };
